#include "boost/describe.hpp"
#include "boost/mp11.hpp"

/*************************************************************
**** ENUMS
**************************************************************/

/// define enums using define macro (no default-values allowed!):
///
/// BOOST_DEFINE_ENUM_CLASS(
///   E,
///   v1,
///   v2,
///   v3,
/// );
///
/// or, (allows default-values):
///
/// enum class E
/// {
///     v1 = 3,
///     v2,
///     v3 = 11
/// };
///
/// BOOST_DESCRIBE_ENUM(E, v1, v2, v3)
template <typename EnumType>
inline const char* cppStringFromEnum(EnumType e) {
    const char* r = "(unnamed)";

    boost::mp11::mp_for_each<boost::describe::describe_enumerators<EnumType>>([&](auto D) {
        if (e == D.value) {
            r = D.name;
        }
    });

    return r;
}
