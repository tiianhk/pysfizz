#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include "sfizz.hpp"

namespace nb = nanobind;

NB_MODULE(pysfizz, m) {
    m.doc() = "Minimal Python bindings for sfizz synthesizer";
    
    m.def("get_sfizz_version", []() -> std::string {
        return "sfizz via pysfizz - minimal build working!";
    });
}

