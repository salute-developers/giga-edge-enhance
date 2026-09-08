# This target is only a named context; only a selected toolchain gets an image output.
target "base" {
  context    = "."
  dockerfile = "dockerfiles/Dockerfile.base"
  platforms  = ["linux/amd64"]
}

target "native" {
  context    = "."
  dockerfile = "dockerfiles/Dockerfile.native"
  platforms  = ["linux/amd64"]
  contexts = {
    spench-base = "target:base"
  }
}

target "web" {
  context    = "."
  dockerfile = "dockerfiles/Dockerfile.web"
  platforms  = ["linux/amd64"]
  contexts = {
    spench-base = "target:base"
  }
}

target "android" {
  context    = "."
  dockerfile = "dockerfiles/Dockerfile.android"
  platforms  = ["linux/amd64"]
  contexts = {
    spench-base = "target:base"
  }
}
