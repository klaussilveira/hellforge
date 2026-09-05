#pragma once

#include "math/Vector3.h"

#include <map>
#include <string>

namespace ui
{

class ThumbnailViewStore
{
public:
    ThumbnailViewStore();

    const Vector3* find(const std::string& key) const;
    void set(const std::string& key, const Vector3& angles);
    void remove(const std::string& key);

private:
    void save() const;

    std::map<std::string, Vector3> _views;
};

}
