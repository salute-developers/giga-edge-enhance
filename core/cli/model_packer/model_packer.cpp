#include <gigadenoiser/packed_model.h>
#include <util/packed_model/packed_model_writer.h>
#include <yaml-cpp/yaml.h>

#include <argparse/argparse.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::vector<uint8_t> read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open file: " + p.string());
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static void write_file(const fs::path& p, const std::vector<uint8_t>& data) {
  std::ofstream out(p, std::ios::binary);
  if (!out) throw std::runtime_error("cannot write output file: " + p.string());
  out.write(reinterpret_cast<const char*>(data.data()), data.size());
}

int32_t main(int32_t argc, char* argv[]) {
  try {
    argparse::ArgumentParser cli("model_packager", "Pack or unpack a GigaDenoiser YAML config with a model");

    cli.add_argument("input").help("YAML configuration file (pack) or packed file (unpack)");
    cli.add_argument("output").help("Output packed file (pack) or output directory (unpack)");

    cli.add_argument("-p", "--pack").help("Run in pack mode").default_value(false).implicit_value(true);
    cli.add_argument("-u", "--unpack").help("Run in unpack mode").default_value(false).implicit_value(true);

    cli.parse_args(argc, argv);

    bool pack_mode = cli.get<bool>("--pack");
    bool unpack_mode = cli.get<bool>("--unpack");

    if (pack_mode == unpack_mode) {
      std::cerr << "Exactly one of --pack or --unpack must be supplied\n";
      return EXIT_FAILURE;
    }

    fs::path input_path = cli.get<std::string>("input");
    fs::path output_path = cli.get<std::string>("output");

    if (pack_mode) {
      // Pack mode – read the YAML, locate the model, and serialize both
      if (!fs::exists(input_path)) throw std::runtime_error("config file does not exist: " + input_path.string());

      // get model path
      YAML::Node yaml_node = YAML::LoadFile(input_path.string());
      if (!yaml_node["model"] || !yaml_node["model"]["model_path"])
        throw std::runtime_error("missing 'model.model_path' in YAML");

      std::string model_path = yaml_node["model"]["model_path"].as<std::string>();
      if (!fs::exists(model_path)) throw std::runtime_error("model file does not exist: " + model_path);

      // remove model_path as it doesn't needed in packed model
      yaml_node["model"].remove("model_path");
      // add the flag that the model is bundled
      yaml_node["model"]["model_bundled"] = true;
      std::string config_text = YAML::Dump(yaml_node);

      std::vector<uint8_t> model_bytes = read_file(model_path);

      // form bundle content
      std::vector<uint8_t> packed_data = gigaedge::denoiser::util::write_packed_model(config_text, model_bytes);

      // write output
      write_file(output_path, packed_data);

      std::cout << "Packed into " << output_path << " (" << packed_data.size() << " bytes)\n";
    } else {  // unpack mode
      if (!fs::exists(input_path)) throw std::runtime_error("packed file does not exist: " + input_path.string());

      std::vector<uint8_t> packed_data = read_file(input_path);

      gigaedge::denoiser::PackedModelReader reader(packed_data);

      // ensure the output directory exists
      fs::create_directories(output_path);

      // write config.yaml
      fs::path cfg_out = output_path / "config.yaml";
      std::ofstream cfg_file(cfg_out, std::ios::binary);
      cfg_file << reader.config();
      cfg_file.close();

      fs::path model_out = output_path / "model.onnx";
      write_file(model_out, reader.model());

      std::cout << "Unpacked to directory: " << output_path << '\n';
      std::cout << "  * config.yaml  -> " << cfg_out << '\n';
      std::cout << "  * model.onnx  -> " << model_out << '\n';
    }

    return EXIT_SUCCESS;
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << '\n';
    return EXIT_FAILURE;
  }
}
