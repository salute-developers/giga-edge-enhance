"""Generate redistributable identity fixtures (no trained weights).

Run with Python packages onnx==1.17.0, flatbuffers==24.3.25, tflite==2.18.0.
These packages are fixture-generation tools, not runtime dependencies.
"""
from pathlib import Path

import flatbuffers
import onnx
from onnx import TensorProto, helper
import tflite

DEST = Path(__file__).parent / "data" / "models"
INPUTS = ["noisy_ri_in", "prev_fb_hidden_in", "prev_fb_snr_noise_psd_in",
          "prev_fb_snr_noise_pg_in", "prev_sb_snr_noise_psd_in", "prev_sb_snr_noise_pg_in",
          "prev_fb_conv1_buf_in", "prev_fb_conv3_buf_in", "prev_stage2_hidden_in"]
OUTPUTS = ["cur_fb_hidden_out", "cur_fb_snr_noise_psd_out", "cur_fb_snr_noise_pg_out",
           "cur_sb_snr_noise_psd_out", "cur_sb_snr_noise_pg_out", "cur_fb_conv1_buf_out",
           "cur_fb_conv3_buf_out", "cur_stage2_hidden_out", "predicted_ri_out"]
SHAPES = [[1, 769, "time", 2]] + [[1, i + 1] for i in range(8)]
ORDER = list(range(1, 9)) + [0]


def make_onnx():
    inputs = [helper.make_tensor_value_info(n, TensorProto.FLOAT, s) for n, s in zip(INPUTS, SHAPES)]
    outputs = [helper.make_tensor_value_info(n, TensorProto.FLOAT, SHAPES[i]) for n, i in zip(OUTPUTS, ORDER)]
    nodes = [helper.make_node("Identity", [INPUTS[i]], [n]) for n, i in zip(OUTPUTS, ORDER)]
    model = helper.make_model(helper.make_graph(nodes, "hybrid_identity", inputs, outputs),
                              opset_imports=[helper.make_opsetid("", 13)], ir_version=8)
    onnx.checker.check_model(model)
    onnx.save(model, DEST / "hybrid_identity.onnx")


def make_tflite():
    b = flatbuffers.Builder(4096)

    def ints(values):
        b.StartVector(4, len(values), 4)
        for value in reversed(values):
            b.PrependInt32(value)
        return b.EndVector()

    def offsets(values):
        b.StartVector(4, len(values), 4)
        for value in reversed(values):
            b.PrependUOffsetTRelative(value)
        return b.EndVector()

    tensors = []
    for name, shape in zip(INPUTS, SHAPES):
        name_offset = b.CreateString(name)
        dims = ints([1 if d == "time" else d for d in shape])
        signature = ints([-1 if d == "time" else d for d in shape])
        tflite.TensorStart(b)
        tflite.TensorAddShape(b, dims)
        tflite.TensorAddShapeSignature(b, signature)
        tflite.TensorAddType(b, tflite.TensorType.FLOAT32)
        tflite.TensorAddBuffer(b, 0)
        tflite.TensorAddName(b, name_offset)
        tensors.append(tflite.TensorEnd(b))
    tensor_vector = offsets(tensors)
    # Deliberately permute the physical order: runners must use signature names.
    inputs = ints([8, 6, 4, 2, 0, 7, 5, 3, 1])
    outputs = ints([3, 1, 7, 5, 0, 2, 8, 4, 6])
    graph_name = b.CreateString("hybrid_identity")
    tflite.SubGraphStart(b)
    tflite.SubGraphAddTensors(b, tensor_vector)
    tflite.SubGraphAddInputs(b, inputs)
    tflite.SubGraphAddOutputs(b, outputs)
    tflite.SubGraphAddName(b, graph_name)
    graph = tflite.SubGraphEnd(b)

    def tensor_map(names, indices):
        maps = []
        for name, index in zip(names, indices):
            key = b.CreateString(name)
            tflite.TensorMapStart(b)
            tflite.TensorMapAddName(b, key)
            tflite.TensorMapAddTensorIndex(b, index)
            maps.append(tflite.TensorMapEnd(b))
        return offsets(maps)

    input_map = tensor_map(INPUTS, range(9))
    output_map = tensor_map(OUTPUTS, ORDER)
    key = b.CreateString("serving_default")
    tflite.SignatureDefStart(b)
    tflite.SignatureDefAddInputs(b, input_map)
    tflite.SignatureDefAddOutputs(b, output_map)
    tflite.SignatureDefAddSignatureKey(b, key)
    tflite.SignatureDefAddSubgraphIndex(b, 0)
    signature = tflite.SignatureDefEnd(b)
    tflite.BufferStart(b)
    buffer = tflite.BufferEnd(b)
    buffers = offsets([buffer])
    graphs = offsets([graph])
    signatures = offsets([signature])
    tflite.ModelStart(b)
    tflite.ModelAddVersion(b, 3)
    tflite.ModelAddSubgraphs(b, graphs)
    tflite.ModelAddBuffers(b, buffers)
    tflite.ModelAddSignatureDefs(b, signatures)
    model = tflite.ModelEnd(b)
    b.Finish(model, file_identifier=b"TFL3")
    (DEST / "hybrid_identity.tflite").write_bytes(b.Output())


if __name__ == "__main__":
    make_onnx()
    make_tflite()
