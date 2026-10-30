#include "package.hpp"

namespace nijigenerate::ext::nodes {

void incInitExtNodes() {
    incRegisterExPart();
    incRegisterExCamera();
    ngRegisterExDepthBoneNodes();
    ngRegisterExGridDeformer();
    ngRegisterExPathDeformer();
}

} // namespace nijigenerate::ext::nodes
