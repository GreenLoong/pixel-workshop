#ifndef MODELRESOURCES_H
#define MODELRESOURCES_H

#include <memory>
#include <vector>

namespace ModelResources {
// 内置模型只读取、校验一次，调用方共享不可修改的字节。
std::shared_ptr<const std::vector<unsigned char>> humanModel();
}
#endif
