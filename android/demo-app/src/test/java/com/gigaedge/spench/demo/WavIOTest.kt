package com.gigaedge.spench.demo

import org.junit.Assert.assertEquals
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataOutputStream

class WavIOTest {
    @Test
    fun readerStopsAtDeclaredDataChunkSize() {
        val wavBytes = wavBytes(
            samples = shortArrayOf(0x0102, 0x0304),
            trailingBytes = byteArrayOf(0x06, 0x05),
        )
        val output = ShortArray(4)

        WavPcm16Reader(ByteArrayInputStream(wavBytes)).use { reader ->
            assertEquals(2, reader.readSamples(output))
            assertEquals(0, reader.readSamples(output))
        }

        assertEquals(0x0102.toShort(), output[0])
        assertEquals(0x0304.toShort(), output[1])
    }

    @Test
    fun readerHandlesOddSizedChunkPaddingBeforeData() {
        val output = ShortArray(1)

        WavPcm16Reader(ByteArrayInputStream(wavBytes(samples = shortArrayOf(0x1234), includeOddJunkChunk = true))).use { reader ->
            assertEquals(1, reader.readSamples(output))
            assertEquals(0x1234.toShort(), output[0])
        }
    }

    private fun wavBytes(
        samples: ShortArray,
        trailingBytes: ByteArray = byteArrayOf(),
        includeOddJunkChunk: Boolean = false,
    ): ByteArray {
        val sampleBytes = ByteArrayOutputStream()
        DataOutputStream(sampleBytes).use { output ->
            for (sample in samples) {
                writeLittleEndianShort(output, sample.toInt())
            }
        }

        val body = ByteArrayOutputStream()
        DataOutputStream(body).use { output ->
            if (includeOddJunkChunk) {
                writeAscii(output, "JUNK")
                writeLittleEndianInt(output, 1)
                output.writeByte(0x7f)
                output.writeByte(0)
            }

            writeAscii(output, "fmt ")
            writeLittleEndianInt(output, 16)
            writeLittleEndianShort(output, 1)
            writeLittleEndianShort(output, 1)
            writeLittleEndianInt(output, 48_000)
            writeLittleEndianInt(output, 96_000)
            writeLittleEndianShort(output, 2)
            writeLittleEndianShort(output, 16)

            writeAscii(output, "data")
            writeLittleEndianInt(output, sampleBytes.size())
            output.write(sampleBytes.toByteArray())
            output.write(trailingBytes)
        }

        return ByteArrayOutputStream().use { bytes ->
            DataOutputStream(bytes).use { output ->
                writeAscii(output, "RIFF")
                writeLittleEndianInt(output, 4 + body.size())
                writeAscii(output, "WAVE")
                output.write(body.toByteArray())
            }
            bytes.toByteArray()
        }
    }

    private fun writeAscii(output: DataOutputStream, text: String) {
        output.write(text.toByteArray(Charsets.US_ASCII))
    }

    private fun writeLittleEndianInt(output: DataOutputStream, value: Int) {
        output.writeByte(value and 0xff)
        output.writeByte((value ushr 8) and 0xff)
        output.writeByte((value ushr 16) and 0xff)
        output.writeByte((value ushr 24) and 0xff)
    }

    private fun writeLittleEndianShort(output: DataOutputStream, value: Int) {
        output.writeByte(value and 0xff)
        output.writeByte((value ushr 8) and 0xff)
    }
}
