#include "VariableManager.h"
#include "../Interfaces.h"
#include "ContainerImpl.h"
#include "../API/Yap/VdfParser.h"

#include <fstream>
#include <string>
#include <vector>

using namespace Yap;
using namespace std;

namespace Yap
{
namespace _details
{
    template <typename VALUE_TYPE>
    class ValueImpl : public IValue<VALUE_TYPE>
    {
    public:
        explicit ValueImpl(VALUE_TYPE value) : _value{ value } {}
        ValueImpl(const ValueImpl& rhs) : _value{ rhs.value } {}
        ValueImpl(const IValue<VALUE_TYPE>& rhs) : _value{ rhs.Get() } {}

        virtual VALUE_TYPE Get() const {
            return _value;
        }

        virtual void Set(const VALUE_TYPE value) {
            _value = value;
        }
    private:
        VALUE_TYPE _value;
    };

    class StringValueImpl : public IStringValue
    {
    public:
        StringValueImpl(const wchar_t * value) : _value{ value } {}
        StringValueImpl(const StringValueImpl& rhs) : _value{ rhs._value } {}
        StringValueImpl(const IStringValue& rhs) : _value{ rhs.Get() } {}

        virtual const wchar_t * Get() const override {
            return _value.c_str();
        }

        virtual void Set(const wchar_t * value) override {
            _value = value;
        }
    private:
        std::wstring _value;
    };

    template <typename VALUE_TYPE>
    class ArrayValueImpl : public IArrayValue<VALUE_TYPE>
    {
    public:
        ArrayValueImpl() {}
        ArrayValueImpl(const ArrayValueImpl& rhs) : _elements(rhs._elements){}
        ArrayValueImpl(const IArrayValue& rhs) : _elements(rhs.GetSize())
        {
            for (size_t i = 0; i < _elements.size(); ++i)
            {
                _elements[i] = const_cast<IArrayValue&>(rhs).Elements()[i];
            }
        }

        ArrayValueImpl(size_t size)
        {
            _elements.resize(size);
        }

        virtual size_t GetSize() const override
        {
            return _elements.size();
        }

        virtual void SetSize(size_t size) override
        {
            _elements.resize(size);
        }

        virtual VALUE_TYPE * Elements() override
        {
            return _elements.data();
        }
    private:
        std::vector<VALUE_TYPE> _elements;
    };

    template<>
    class ArrayValueImpl<bool> : public IArrayValue<bool>
    {
    public:
        ArrayValueImpl()
        {}

        ArrayValueImpl(const ArrayValueImpl<bool>& rhs) : _elements(rhs.GetSize()){
            for (size_t i = 0; i < _elements.size(); ++i)
            {
                _elements[i] = const_cast<ArrayValueImpl<bool>&>(rhs).Elements()[i];
            }
        }

        ArrayValueImpl(const IArrayValue<bool>& rhs) : _elements(rhs.GetSize()){
            for (size_t i = 0; i < _elements.size(); ++i)
            {
                _elements[i] = const_cast<IArrayValue<bool>&>(rhs).Elements()[i];
            }
        }

        ArrayValueImpl(size_t size)
        {
            _elements.resize(size);
        }

        virtual size_t GetSize() const override
        {
            return _elements.size();
        }

        virtual void SetSize(size_t size) override
        {
            _elements.resize(size);
        }

        virtual bool * Elements() override
        {
            return reinterpret_cast<bool*>(_elements.data());
        }

    private:
        std::vector<unsigned char> _elements;
    };

    template<>
    class ArrayValueImpl<IProperty*> : public IArrayValue<IProperty*>
    {
    public:
        ArrayValueImpl()
        {}

        ArrayValueImpl(const ArrayValueImpl<IProperty*>& rhs) : _elements(rhs.GetSize())
        {
            for (size_t i = 0; i < _elements.size(); ++i)
            {
                _elements[i] = dynamic_cast<IProperty*>(const_cast<ArrayValueImpl<IProperty*>&>(rhs).Elements()[i]->Clone());
                _elements[i]->Lock();
            }
        }

        ArrayValueImpl(const IArrayValue<IProperty*>& rhs) : _elements(rhs.GetSize())
        {
            for (size_t i = 0; i < _elements.size(); ++i)
            {
                _elements[i] = dynamic_cast<IProperty*>(const_cast<IArrayValue<IProperty*>&>(rhs).Elements()[i]->Clone());
                _elements[i]->Lock();
            }
        }

        ArrayValueImpl(size_t size)
        {
            _elements.resize(size);
        }

        ~ArrayValueImpl()
        {
            for (auto element : _elements)
            {
                element->Release();
            }
        }

        virtual size_t GetSize() const override
        {
            return _elements.size();
        }

        virtual void SetSize(size_t size) override
        {
            _elements.resize(size);
        }

        virtual IProperty ** Elements() override
        {
            return reinterpret_cast<IProperty**>(_elements.data());
        }

    private:
        std::vector<IProperty*> _elements;
    };

    class PropertyImpl :
        public IProperty
    {
        IMPLEMENT_SHARED(PropertyImpl)

    public:
        PropertyImpl(PropertyType type,
            const wchar_t * name,
            const wchar_t * description) :
            _type(type),
            _name(name),
            _description(description != nullptr ? description : L"")
        {
            switch (type)
            {
            case PropertyBool:
                _value_interface = new ValueImpl<bool>(false);
                break;
            case PropertyInt:
                _value_interface = new ValueImpl<int>(0);
                break;
            case PropertyFloat:
                _value_interface = new ValueImpl<double>(0.0);
                break;
            case PropertyString:
                _value_interface = new StringValueImpl(L"");
                break;
            case PropertyBoolArray:
                _value_interface = new ArrayValueImpl<bool>();
                break;
            case PropertyIntArray:
                _value_interface = new ArrayValueImpl<int>();
                break;
            case PropertyFloatArray:
                _value_interface = new ArrayValueImpl<double>();
                break;
            case PropertyStringArray:
                _value_interface = new ArrayValueImpl<std::wstring>();
                break;
            case PropertyStruct:
                _value_interface = nullptr;
                break;
            case PropertyStructArray:
                _value_interface = new ArrayValueImpl<IProperty*>();
                break;
            default:
                throw PropertyException(name, PropertyException::InvalidType);
            }
        }

        PropertyImpl(const IProperty& rhs) :
            _type{ rhs.GetType() },
            _name{ rhs.GetName() },
            _description{ rhs.GetDescription() }
        {
            auto rhs_value_interface = const_cast<IProperty&>(rhs).ValueInterface();
            switch (rhs.GetType())
            {
            case PropertyBool:
                _value_interface = new ValueImpl<bool>(
                    *reinterpret_cast<IBoolValue*>(rhs_value_interface));
                break;
            case PropertyInt:
                _value_interface = new ValueImpl<int>(
                    *reinterpret_cast<IIntValue*>(rhs_value_interface));
                break;
            case PropertyFloat:
                _value_interface = new ValueImpl<double>(
                    *reinterpret_cast<IDoubleValue*>(rhs_value_interface));
                break;
            case PropertyString:
                _value_interface = new StringValueImpl(
                    *reinterpret_cast<IStringValue*>(rhs_value_interface));
                break;
            case PropertyStruct:
                _value_interface = reinterpret_cast<IStructValue*>(rhs_value_interface)->Clone();
                break;
            case PropertyBoolArray:
                _value_interface = new ArrayValueImpl<bool>(
                    *reinterpret_cast<IArrayValue<bool>*>(rhs_value_interface));
                break;
            case PropertyIntArray:
                _value_interface = new ArrayValueImpl<int>(
                    *reinterpret_cast<IArrayValue<int>*>(rhs_value_interface));
                break;
            case PropertyFloatArray:
                _value_interface = new ArrayValueImpl<double>(
                    *reinterpret_cast<IArrayValue<double>*>(rhs_value_interface));
                break;
            case PropertyStringArray:
                _value_interface = new ArrayValueImpl<wstring>(
                    *reinterpret_cast<IArrayValue<wstring>*>(rhs_value_interface));
                break;
            case PropertyStructArray:
                _value_interface = new ArrayValueImpl<IProperty*>(
                    *reinterpret_cast<IArrayValue<IProperty*>*>(rhs_value_interface));
                break;
            default:
                throw PropertyException(_name.c_str(), PropertyException::InvalidType);

            }
        }

        ~PropertyImpl()
        {
            switch (_type)
            {
            case PropertyBool:
            case PropertyInt:
            case PropertyFloat:
            case PropertyString:
            case PropertyBoolArray:
            case PropertyIntArray:
            case PropertyStringArray:
            case PropertyFloatArray:
                delete _value_interface;
                break;
            case PropertyStruct:
                if (_value_interface != nullptr)
                {
                    reinterpret_cast<IStructValue*>(_value_interface)->Release();
                }
                break;
            case PropertyStructArray:
                BUG(Not implemented yet.);
                break;
            }
        }

        virtual PropertyType GetType() const override
        {
            return _type;
        }

        virtual const wchar_t * GetName() const override
        {
            return _name.c_str();
        }

        virtual void SetName(const wchar_t * name) override
        {
            _name = name;
        }

        virtual const wchar_t * GetDescription() const override
        {
            return _description.c_str();
        }

        virtual void SetDescription(const wchar_t * description) override
        {
            _description = description;
        }

        virtual void * ValueInterface() override
        {
            return _value_interface;
        }

    protected:
        std::wstring _name;
        std::wstring _description;
        PropertyType _type;

        void * _value_interface;
        friend class VariableManager;
    };

}  // end Yap::_details

using namespace _details;

VariableManager::VariableManager() :
	_properties(YapShared(new ContainerImpl<IProperty>))
{
    InitTypes();
}

VariableManager::VariableManager(IPropertyContainer * properties) :
	_properties(YapShared(properties))
{
    InitTypes();
}

VariableManager::VariableManager(const VariableManager& rhs) :
	_properties(YapShared(rhs.GetProperties()->Clone()))
{
    InitTypes();
}

VariableManager::~VariableManager()
{
}

bool VariableManager::InitTypes()
{
    _types.insert(make_pair(L"int", YapShared(new PropertyImpl(PropertyInt, L"int", nullptr))));
    _types.insert(make_pair(L"float", YapShared(new PropertyImpl(PropertyFloat, L"float", nullptr))));
    _types.insert(make_pair(L"string", YapShared(new PropertyImpl(PropertyString, L"string", nullptr))));
    _types.insert(make_pair(L"bool", YapShared(new PropertyImpl(PropertyBool, L"bool", nullptr))));

    return true;
}

using namespace _details;

bool VariableManager::AddProperty(PropertyType type,
	const wchar_t * name,
	const wchar_t * description)
{
	try
	{
        auto new_property = new PropertyImpl(type, name, description);
        _properties->Add(name, new_property);
		return true;
	}
	catch (std::bad_alloc&)
	{
		return false;
	}
}

bool VariableManager::AddProperty(const wchar_t * type,
                                   const wchar_t * name,
                                   const wchar_t * description)
{
    auto iter = _types.find(type);
    if (iter == _types.end())
        return false;

    if (!iter->second)
        return false;

    auto new_property = dynamic_cast<IProperty*>(iter->second->Clone());
    if (new_property == nullptr)
        return false;

    new_property->SetName(name);
    new_property->SetDescription(description);
    _properties->Add(name, new_property);

    return true;
}

bool VariableManager::AddProperty(IProperty* property)
{
    assert(property != nullptr);
    return _properties->Add(property->GetName(), property);
}

IPropertyContainer* VariableManager::GetProperties()
{
	return _properties.get();
}

const IPropertyContainer* VariableManager::GetProperties() const
{
	return _properties.get();
}

IProperty * VariableManager::GetProperty(const wchar_t * name, PropertyType expected_type)
{
	assert(name != nullptr && name[0] != 0);

	auto property = _properties->Find(name);
	if (property == nullptr)
		throw PropertyException(name, PropertyException::PropertyNotFound);

	if (property->GetType() != expected_type)
		throw PropertyException(name, PropertyException::TypeNotMatch);

	return property;
}

void VariableManager::SetInt(const wchar_t * name, int value)
{
	auto property = GetProperty(name, PropertyInt);
	assert(property->ValueInterface() != nullptr);
	reinterpret_cast<IIntValue*>(property->ValueInterface())->Set(value);
}

int VariableManager::GetInt(const wchar_t * name)
{
	auto property = GetProperty(name, PropertyInt);
	assert(property->ValueInterface() != nullptr);

	return reinterpret_cast<IIntValue*>(property->ValueInterface())->Get();
}

void VariableManager::SetFloat(const wchar_t * name, double value)
{
	auto property = GetProperty(name, PropertyFloat);
	assert(property->ValueInterface() != nullptr);

	reinterpret_cast<IDoubleValue*>(property->ValueInterface())->Set(value);
}

double VariableManager::GetFloat(const wchar_t * name)
{
	auto property = GetProperty(name, PropertyFloat);
	assert(property->ValueInterface() != nullptr);

	return reinterpret_cast<IDoubleValue*>(property->ValueInterface())->Get();
}

void VariableManager::SetBool(const wchar_t * name, bool value)
{
	auto property = GetProperty(name, PropertyBool);
	assert(property->ValueInterface() != nullptr);

	reinterpret_cast<IBoolValue*>(property->ValueInterface())->Set(value);
}

bool VariableManager::GetBool(const wchar_t * name)
{
	auto property = GetProperty(name, PropertyBool);
	assert(property->ValueInterface() != nullptr);

	return reinterpret_cast<IBoolValue*>(property->ValueInterface())->Get();
}

void VariableManager::SetString(const wchar_t * name, const wchar_t * value)
{
	auto property = GetProperty(name, PropertyString);
	assert(property->ValueInterface() != nullptr);

	reinterpret_cast<IStringValue*>(property->ValueInterface())->Set(value);
}


const wchar_t * VariableManager::GetString(const wchar_t * name)
{
	auto property = GetProperty(name, PropertyString);
	assert(property->ValueInterface() != nullptr);

	return reinterpret_cast<IStringValue*>(property->ValueInterface())->Get();
}

shared_ptr<VariableManager> VariableManager::Load(const wchar_t * path)
{
    VdfParser parser;
    return parser.CompileFile(path);
}

void VariableManager::Reset()
{
	_properties = YapShared(new ContainerImpl<IProperty>);
}

bool VariableManager::TypeExists(const wchar_t * type) const
{
    return _types.find(type) != _types.end();
}

bool VariableManager::PropertyExists(const wchar_t *property_id) const
{
    auto This = const_cast<VariableManager*>(this);
    return This->_properties->Find(property_id) != nullptr;
}

const IProperty * VariableManager::GetType(const wchar_t * type) const
{
    auto iter = _types.find(type);
    return (iter != _types.end()) ? iter->second.get() : nullptr;
}

bool VariableManager::AddType(const wchar_t * type_id, IContainer<IProperty> * properties)
{
    try
    {
        auto property = new PropertyImpl(PropertyStruct, type_id, nullptr);
        properties->Lock();
        property->_value_interface = properties;
        return AddType(type_id, property);
    }
    catch(bad_alloc&)
    {
        return false;
    }
}

bool VariableManager::AddType(const wchar_t * type_id, IProperty *type)
{
    assert(_types.find(type_id) == _types.end());
    assert(type != nullptr);
    assert(type_id != nullptr);

    if (_types.find(type_id) != _types.end())
        return false;

    _types.insert({type_id, YapShared(type)});

    return true;
}

}	// end Yap