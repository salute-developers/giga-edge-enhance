package com.gigaedge.enhance.demo

import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.ByteArrayOutputStream
import java.io.Closeable
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.EOFException
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.InputStream
import java.io.RandomAccessFile

internal data class WavFormat(
    val sampleRateHz: Int,
    val channelCount: Int,
    val bitsPerSample: Int,
    val dataSizeBytes: Int,
)

internal class WavPcm16Reader(inputStream: InputStream) : Closeable {
    private val input = DataInputStream(BufferedInputStream(inputStream))
    val format: WavFormat = readHeader(input)
    private var remainingDataBytes = format.dataSizeBytes

    init {
        require(format.channelCount == 1) { "Only mono WAV files are supported" }
        require(format.bitsPerSample == 16) { "Only 16-bit PCM WAV files are supported" }
        require(format.dataSizeBytes % BYTES_PER_SAMPLE == 0) { "Invalid PCM data size" }
    }

    fun readSamples(buffer: ShortArray): Int {
        var count = 0
        val maxSamples = minOf(buffer.size, remainingDataBytes / BYTES_PER_SAMPLE)
        while (count < maxSamples) {
            val low = input.read()
            if (low == -1) {
                throw EOFException("Unexpected EOF while reading WAV data")
            }
            val high = input.read()
            if (high == -1) {
                throw EOFException("Unexpected EOF while reading WAV data")
            }
            buffer[count] = (((high and 0xff) shl 8) or (low and 0xff)).toShort()
            count++
            remainingDataBytes -= BYTES_PER_SAMPLE
        }
        return count
    }

    override fun close() {
        input.close()
    }

    private companion object {
        const val BYTES_PER_SAMPLE = 2

        fun readHeader(input: DataInputStream): WavFormat {
            require(readAscii(input, 4) == "RIFF") { "Invalid WAV header" }
            readLittleEndianInt(input)
            require(readAscii(input, 4) == "WAVE") { "Invalid WAV format" }

            var sampleRate = 0
            var channelCount = 0
            var bitsPerSample = 0
            var dataSize = 0

            while (true) {
                val chunkId = readAscii(input, 4)
                val chunkSize = readLittleEndianInt(input)
                require(chunkSize >= 0) { "Invalid WAV chunk size" }
                when (chunkId) {
                    "fmt " -> {
                        require(chunkSize >= 16) { "Invalid fmt chunk size" }
                        val audioFormat = readLittleEndianShort(input).toInt() and 0xffff
                        channelCount = readLittleEndianShort(input).toInt() and 0xffff
                        sampleRate = readLittleEndianInt(input)
                        readLittleEndianInt(input)
                        readLittleEndianShort(input)
                        bitsPerSample = readLittleEndianShort(input).toInt() and 0xffff
                        require(audioFormat == 1) { "Only PCM WAV files are supported" }
                        skipChunkRemainder(input, chunkSize, bytesRead = 16)
                    }
                    "data" -> {
                        dataSize = chunkSize
                        break
                    }
                    else -> skipChunk(input, chunkSize)
                }
            }

            require(sampleRate > 0) { "Invalid sample rate" }
            require(channelCount > 0) { "Invalid channel count" }
            require(bitsPerSample > 0) { "Invalid bits per sample" }
            require(dataSize > 0) { "Invalid data size" }
            return WavFormat(sampleRate, channelCount, bitsPerSample, dataSize)
        }

        fun readAscii(input: DataInputStream, size: Int): String {
            val bytes = ByteArray(size)
            input.readFully(bytes)
            return bytes.toString(Charsets.US_ASCII)
        }

        fun readLittleEndianInt(input: DataInputStream): Int {
            val b0 = input.readUnsignedByte()
            val b1 = input.readUnsignedByte()
            val b2 = input.readUnsignedByte()
            val b3 = input.readUnsignedByte()
            return b0 or (b1 shl 8) or (b2 shl 16) or (b3 shl 24)
        }

        fun readLittleEndianShort(input: DataInputStream): Short {
            val b0 = input.readUnsignedByte()
            val b1 = input.readUnsignedByte()
            return ((b1 shl 8) or b0).toShort()
        }

        fun skipChunkRemainder(input: DataInputStream, chunkSize: Int, bytesRead: Int) {
            skipChunk(input, chunkSize - bytesRead)
        }

        fun skipChunk(input: DataInputStream, chunkSize: Int) {
            exactSkip(input, chunkSize + (chunkSize and 1))
        }

        fun exactSkip(input: DataInputStream, byteCount: Int) {
            var remaining = byteCount
            while (remaining > 0) {
                val skipped = input.skipBytes(remaining)
                if (skipped > 0) {
                    remaining -= skipped
                    continue
                }
                if (input.read() == -1) {
                    throw EOFException("Unexpected EOF while skipping WAV data")
                }
                remaining--
            }
        }
    }
}

internal class WavPcm16Writer(
    private val outputFile: File,
    private val sampleRateHz: Int,
    private val channelCount: Int,
) : Closeable {
    private val output = DataOutputStream(BufferedOutputStream(FileOutputStream(outputFile)))
    private var dataSizeBytes = 0

    init {
        writeHeaderPlaceholder()
    }

    fun writeSamples(buffer: ShortArray, size: Int) {
        for (i in 0 until size) {
            val sample = buffer[i].toInt()
            output.writeByte(sample and 0xff)
            output.writeByte((sample ushr 8) and 0xff)
            dataSizeBytes += 2
        }
    }

    override fun close() {
        output.flush()
        output.close()
        rewriteHeader(outputFile, sampleRateHz, channelCount, dataSizeBytes)
    }

    private fun writeHeaderPlaceholder() {
        repeat(WAV_HEADER_SIZE_BYTES) {
            output.writeByte(0)
        }
    }

    private companion object {
        const val WAV_HEADER_SIZE_BYTES = 44

        fun rewriteHeader(file: File, sampleRateHz: Int, channelCount: Int, dataSizeBytes: Int) {
            val headerBytes = ByteArrayOutputStream(WAV_HEADER_SIZE_BYTES)
            DataOutputStream(headerBytes).use { output ->
                val byteRate = sampleRateHz * channelCount * 2
                val blockAlign = channelCount * 2
                val riffChunkSize = 36 + dataSizeBytes

                writeAscii(output, "RIFF")
                writeLittleEndianInt(output, riffChunkSize)
                writeAscii(output, "WAVE")
                writeAscii(output, "fmt ")
                writeLittleEndianInt(output, 16)
                writeLittleEndianShort(output, 1)
                writeLittleEndianShort(output, channelCount)
                writeLittleEndianInt(output, sampleRateHz)
                writeLittleEndianInt(output, byteRate)
                writeLittleEndianShort(output, blockAlign)
                writeLittleEndianShort(output, 16)
                writeAscii(output, "data")
                writeLittleEndianInt(output, dataSizeBytes)
            }

            RandomAccessFile(file, "rw").use { randomAccessFile ->
                randomAccessFile.seek(0)
                randomAccessFile.write(headerBytes.toByteArray())
            }
        }

        fun writeAscii(output: DataOutputStream, text: String) {
            output.write(text.toByteArray(Charsets.US_ASCII))
        }

        fun writeLittleEndianInt(output: DataOutputStream, value: Int) {
            output.writeByte(value and 0xff)
            output.writeByte((value ushr 8) and 0xff)
            output.writeByte((value ushr 16) and 0xff)
            output.writeByte((value ushr 24) and 0xff)
        }

        fun writeLittleEndianShort(output: DataOutputStream, value: Int) {
            output.writeByte(value and 0xff)
            output.writeByte((value ushr 8) and 0xff)
        }
    }
}

internal fun copyFileToStream(source: File, outputStream: java.io.OutputStream) {
    FileInputStream(source).use { input ->
        outputStream.use { output ->
            input.copyTo(output)
            output.flush()
        }
    }
}
