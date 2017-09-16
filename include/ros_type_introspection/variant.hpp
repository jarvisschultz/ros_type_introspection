#ifndef VARIANT_H
#define VARIANT_H

#include <type_traits>
#include <limits>
#include "ros_type_introspection/builtin_types.hpp"
#include "ros_type_introspection/string.hpp"
#include "ros_type_introspection/details/exceptions.hpp"
#include "ros_type_introspection/details/conversion_impl.hpp"


namespace RosIntrospection
{

#if 1
// Faster, but might need more testing
typedef ssoX::basic_string<char> SString;
#else
// slightly slower but safer option. More convenient during debug
typedef std::string SString;
#endif

class Variant
{

public:

  Variant() {
    _type = OTHER;
  }

  ~Variant();

  template<typename T> Variant(const T& value);

  // specialization for raw string
  Variant(const char* buffer, size_t length);

  BuiltinType getTypeID() const;

  template<typename T> T convert( ) const;

  template<typename T> T extract( ) const;

  template <typename T> void assign(const T& value);

  void assign(const char* buffer, size_t length);

private:

  union {
    std::array<uint8_t,8> raw_data;
    char* raw_string;
  }_storage;

  BuiltinType _type;
};

template <typename T> inline
bool operator ==(const Variant& var, const T& num)
{
  return var.convert<T>() == num;
}

template <typename T> inline
bool operator ==(const T& num, const Variant& var)
{
  return var.convert<T>() == num;
}

//----------------------- Implementation ----------------------------------------------


template<typename T>
inline Variant::Variant(const T& value):
  _type(OTHER)
{
  static_assert (std::numeric_limits<T>::is_specialized ||
                 std::is_same<T, ros::Time>::value ||
                 std::is_same<T, SString>::value ||
                 std::is_same<T, std::string>::value ||
                 std::is_same<T, ros::Duration>::value
                 , "not a valid type");

  _storage.raw_string = (nullptr);
  assign(value);
}

inline Variant::Variant(const char* buffer, size_t length):_type(OTHER)
{
  _storage.raw_string = (nullptr);
  assign(buffer,length);
}

inline Variant::~Variant()
{
  if( _storage.raw_string && _type == STRING)
  {
    delete [] _storage.raw_string;
  }
}

//-------------------------------------

inline BuiltinType Variant::getTypeID() const {
  return _type;
}

template<typename T> inline T Variant::extract( ) const
{
  static_assert (std::numeric_limits<T>::is_specialized ||
                 std::is_same<T, ros::Time>::value ||
                 std::is_same<T, ros::Duration>::value
                 , "not a valid type");

  if( _type != RosIntrospection::getType<T>() )
  {
    throw TypeException("Variant::extract -> wrong type");
  }
  return * reinterpret_cast<const T*>( &_storage.raw_data[0] );
}

template<> inline SString Variant::extract( ) const
{

  if( _type != STRING )
  {
    throw TypeException("Variant::extract -> wrong type");
  }
  const uint32_t size = *(reinterpret_cast<const uint32_t*>( &_storage.raw_string[0] ));
  char* data = static_cast<char*>(&_storage.raw_string[4]);
  return SString(data, size);
}

template<> inline std::string Variant::extract( ) const
{
  if( _type != STRING )
  {
    throw TypeException("Variant::extract -> wrong type");
  }
  const uint32_t size = *(reinterpret_cast<const uint32_t*>( &_storage.raw_string[0] ));
  char* data = static_cast<char*>(&_storage.raw_string[4]);
  return std::string(data, size);
}

//-------------------------------------

template <typename T> inline void Variant::assign(const T& value)
{
  static_assert (std::numeric_limits<T>::is_specialized ||
                 std::is_same<T, ros::Time>::value ||
                 std::is_same<T, ros::Duration>::value
                 , "not a valid type");

  if( _storage.raw_string && _type == STRING)
  {
    delete [] _storage.raw_string;
    _storage.raw_string = nullptr;
  }

  _type = RosIntrospection::getType<T>() ;
  *reinterpret_cast<T *>( &_storage.raw_data[0] ) =  value;
}

inline void Variant::assign(const char* buffer, size_t size)
{
  if( _storage.raw_string && _type == STRING)
  {
    delete [] _storage.raw_string;
    _storage.raw_string = nullptr;
  }

  _type = STRING;

  _storage.raw_string = new char[size+5];
  *reinterpret_cast<uint32_t *>( &_storage.raw_string[0] ) = size;
  memcpy(&_storage.raw_string[4] , buffer, size );
  _storage.raw_string[size-1] = '\0';
}


template <> inline void Variant::assign(const SString& value)
{
  if( _storage.raw_string && _type == STRING)
  {
    delete [] _storage.raw_string;
    _storage.raw_string = nullptr;
  }
  _type = STRING;
  const uint32_t size = static_cast<uint32_t>(value.size());
  _storage.raw_string = new char[size+5];
  *reinterpret_cast<uint32_t *>( &_storage.raw_string[0] ) = size;
  memcpy(&_storage.raw_string[4] , value.data(), size );
  _storage.raw_string[size-1] = '\0';
}

template <> inline void Variant::assign(const std::string& value)
{
  if( _storage.raw_string && _type == STRING)
  {
    delete [] _storage.raw_string;
    _storage.raw_string = nullptr;
  }
  _type = STRING;
  const uint32_t size = static_cast<uint32_t>(value.size());
  _storage.raw_string = new char[size+4];
  *reinterpret_cast<uint32_t *>( &_storage.raw_string[0] ) = size;
  memcpy(&_storage.raw_string[4] , value.data(), size );
}

//-------------------------------------

template<typename DST> inline DST Variant::convert() const
{
  static_assert (std::numeric_limits<DST>::is_specialized ||
                 std::is_same<DST, ros::Time>::value ||
                 std::is_same<DST, ros::Duration>::value
                 , "not a valid type");

  using namespace RosIntrospection::details;
  DST target;

  const auto& raw_data = &_storage.raw_data[0];
  //----------
  switch( _type )
  {
  case CHAR:
  case INT8:   convert_impl<int8_t,  DST>(*reinterpret_cast<const int8_t*>( raw_data), target  ); break;

  case INT16:  convert_impl<int16_t, DST>(*reinterpret_cast<const int16_t*>( raw_data), target  ); break;
  case INT32:  convert_impl<int32_t, DST>(*reinterpret_cast<const int32_t*>( raw_data), target  ); break;
  case INT64:  convert_impl<int64_t, DST>(*reinterpret_cast<const int64_t*>( raw_data), target  ); break;

  case BOOL:
  case BYTE:
  case UINT8:   convert_impl<uint8_t,  DST>(*reinterpret_cast<const uint8_t*>( raw_data), target  ); break;

  case UINT16:  convert_impl<uint16_t, DST>(*reinterpret_cast<const uint16_t*>( raw_data), target  ); break;
  case UINT32:  convert_impl<uint32_t, DST>(*reinterpret_cast<const uint32_t*>( raw_data), target  ); break;
  case UINT64:  convert_impl<uint64_t, DST>(*reinterpret_cast<const uint64_t*>( raw_data), target  ); break;

  case FLOAT32:  convert_impl<float, DST>(*reinterpret_cast<const float*>( raw_data), target  ); break;
  case FLOAT64:  convert_impl<double, DST>(*reinterpret_cast<const double*>( raw_data), target  ); break;

  case STRING: {
    throw TypeException("String will not be converted to a numerical value implicitly");
 } break;

  case DURATION:
  case TIME: {
     throw TypeException("ros::Duration and ros::Time can be converted only to double (will be seconds)");
  } break;

  case OTHER: throw TypeException("Variant::convert -> cannot convert type" + std::to_string(_type)); break;

  }
  return  target;
}

template<> inline double Variant::convert() const
{
  using namespace RosIntrospection::details;
  double target = 0;
  const auto& raw_data = &_storage.raw_data[0];
  //----------
  switch( _type )
  {
  case CHAR:
  case INT8:   convert_impl<int8_t,  double>(*reinterpret_cast<const int8_t*>( raw_data), target  ); break;

  case INT16:  convert_impl<int16_t, double>(*reinterpret_cast<const int16_t*>( raw_data), target  ); break;
  case INT32:  convert_impl<int32_t, double>(*reinterpret_cast<const int32_t*>( raw_data), target  ); break;
  case INT64:  convert_impl<int64_t, double>(*reinterpret_cast<const int64_t*>( raw_data), target  ); break;

  case BOOL:
  case BYTE:
  case UINT8:   convert_impl<uint8_t,  double>(*reinterpret_cast<const uint8_t*>( raw_data), target  ); break;

  case UINT16:  convert_impl<uint16_t, double>(*reinterpret_cast<const uint16_t*>( raw_data), target  ); break;
  case UINT32:  convert_impl<uint32_t, double>(*reinterpret_cast<const uint32_t*>( raw_data), target  ); break;
  case UINT64:  convert_impl<uint64_t, double>(*reinterpret_cast<const uint64_t*>( raw_data), target  ); break;

  case FLOAT32:  convert_impl<float, double>(*reinterpret_cast<const float*>( raw_data), target  ); break;
  case FLOAT64:  convert_impl<double, double>(*reinterpret_cast<const double*>( raw_data), target  ); break;

  case STRING: {
    throw TypeException("String will not be converted to a double implicitly");
  }break;

  case DURATION: {
    ros::Duration tmp =  extract<ros::Duration>();
    target = tmp.toSec();
  }break;

  case TIME: {
    ros::Time tmp =  extract<ros::Time>();
    target = tmp.toSec();
  }break;

  case OTHER: throw TypeException("Variant::convert -> cannot convert type" + std::to_string(_type));

  }
  return  target;
}

template<> inline ros::Time Variant::convert() const
{
  if(  _type != TIME )
  {
     throw TypeException("Variant::convert -> cannot convert ros::Time");
  }
  return extract<ros::Time>();
}

template<> inline ros::Duration Variant::convert() const
{
  if(  _type != DURATION )
  {
     throw TypeException("Variant::convert -> cannot convert ros::Duration");
  }
  return extract<ros::Duration>();
}

template<> inline SString Variant::convert() const
{
  if(  _type != STRING )
  {
     throw TypeException("Variant::convert -> cannot convert to SString");
  }
  return extract<SString>();
}

template<> inline std::string Variant::convert() const
{
  if(  _type != STRING )
  {
     throw TypeException("Variant::convert -> cannot convert to std::string");
  }
  return extract<std::string>();
}

} //end namespace


#endif // VARIANT_H
