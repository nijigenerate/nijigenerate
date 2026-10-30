#pragma once
#include <nodes/part.hpp>
#include <memory>

namespace nijigenerate::viewport::common::mesheditor::brushstate {
using TeacherPart = nicxlive::core::nodes::Part;
std::shared_ptr<TeacherPart> ngBrushGetTeacherPart();
void ngBrushSetTeacherPart(std::shared_ptr<TeacherPart> part);
void ngBrushClearTeacherPart();
bool ngBrushHasTeacherPart();
}
