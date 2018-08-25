#pragma once

#include <vector>
#include <memory>

namespace dot {
class DotGraphNodeType
{
public:
    using DotGraphNodeType_ptr = std::shared_ptr<DotGraphNodeType>;
    virtual std::vector<DotGraphNodeType_ptr> get_connections() const
    {
        return std::vector<DotGraphNodeType_ptr>();
    }

    virtual std::string get_id() const = 0;
    virtual std::string get_label() const = 0;
};

}

