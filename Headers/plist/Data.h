/**
 Copyright (c) 2015-present, Facebook, Inc.
 All rights reserved.

 This source code is licensed under the BSD-style license found in the
 LICENSE file in the root directory of this source tree. An additional grant
 of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef __plist_Data_h
#define __plist_Data_h

#include <plist/Base.h>
#include <plist/Object.h>

namespace plist {

class Data : public Object {
private:
    std::vector <uint8_t> _value;

public:
    Data(std::vector <uint8_t> const &value = std::vector <uint8_t> ()) :
        _value(value)
    {
    }

    Data(std::vector <uint8_t> &&value) :
        _value(value)
    {
    }

    Data(std::string const &value)
    {
        libutil::Base64::Decode(value, _value);
    }

    Data(void const *bytes, size_t length)
    {
        setValue(bytes, length);
    }

public:
    inline std::vector <uint8_t> const &value() const
    {
        return _value;
    }

    inline void setValue(std::vector <uint8_t> const &value)
    {
        _value = value;
    }

    inline void setValue(std::vector <uint8_t> &&value)
    {
        _value = value;
    }

public:
    inline void setValue(void const *bytes, size_t length)
    {
        _value.resize(length);
        std::memcpy(&_value[0], bytes, length);
    }


public:
    inline void setBase64Value(std::string const &value)
    {
        libutil::Base64::Decode(value, _value);
    }

    inline std::string base64Value() const
    {
        return libutil::Base64::Encode(_value);
    }

public:
    static std::unique_ptr<Data> New(std::vector<uint8_t> const &value = std::vector<uint8_t>());
    static std::unique_ptr<Data> New(std::vector<uint8_t> &&value);
    static std::unique_ptr<Data> New(std::string const &value);
    static std::unique_ptr<Data> New(void const *bytes, size_t length);

public:
    static std::unique_ptr<Data> Coerce(Object const *obj);

public:
    virtual enum Object::Type type() const
    {
        return Data::Type();
    }

    static inline enum Object::Type Type()
    {
        return Object::kTypeData;
    }

protected:
    virtual std::unique_ptr<Object> _copy() const;

public:
    std::unique_ptr<Data> copy() const
    { return libutil::static_unique_pointer_cast<Data>(_copy()); }

public:
    virtual bool equals(Object const *obj) const
    {
        if (Object::equals(obj))
            return true;

        Data const *objt = CastTo <Data> (obj);
        return (objt != nullptr && equals(objt));
    }

    virtual bool equals(Data const *obj) const
    {
        return (obj != nullptr && (obj == this || value() == obj->value()));
    }
};

}

#endif  // !__plist_Data_h