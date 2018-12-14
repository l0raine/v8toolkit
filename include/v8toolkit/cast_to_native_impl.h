
#pragma once

#include <chrono>

#include <xl/demangle.h>

#include "cast_to_native.h"
#include "call_javascript_function.h"
#include "v8helpers.h"

namespace v8toolkit {


/**
* Casts from a boxed Javascript type to a native type
*/
template<typename T, typename Behavior, typename>
struct CastToNative {
    template<class U = T> // just to make it dependent so the static_asserts don't fire before `callable` can be called
    T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        static_assert(!std::is_pointer<T>::value, "Cannot CastToNative to a pointer type of an unwrapped type");
        static_assert(!(std::is_lvalue_reference<T>::value && !std::is_const<std::remove_reference_t<T>>::value),
                      "Cannot CastToNative to a non-const "
                          "lvalue reference of an unwrapped type because there is no lvalue variable to send");
        static_assert(!is_wrapped_type_v<T>,
                      "CastToNative<SomeWrappedType> shouldn't fall through to this specialization");
        static_assert(always_false_v<T>, "Invalid CastToNative configuration - maybe an unwrapped type without a CastToNative defined for it?");
    }
    
    static constexpr bool callable(){return false;}
};


template<typename T, typename Behavior>
struct CastToNative<T, Behavior, std::enable_if_t<std::is_enum_v<T>>> {
    T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return T(static_cast<T>(value->ToNumber(isolate->GetCurrentContext()).ToLocalChecked()->Value()));
    }
};


template<typename Behavior>
struct CastToNative<void, Behavior> {
    void operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {}
    static constexpr bool callable(){return true;}
};




CAST_TO_NATIVE(bool, {return static_cast<bool>(value->ToBoolean(isolate->GetCurrentContext()).ToLocalChecked()->Value());});


// integers
CAST_TO_NATIVE(long long, {return static_cast<long long>(value->ToInteger(isolate->GetCurrentContext()).ToLocalChecked()->Value());});
CAST_TO_NATIVE(unsigned long long, {return static_cast<unsigned long long>(value->ToInteger(isolate->GetCurrentContext()).ToLocalChecked()->Value());});

CAST_TO_NATIVE(long, {return static_cast<long>(value->ToInteger(isolate->GetCurrentContext()).ToLocalChecked()->Value());});
CAST_TO_NATIVE(unsigned long, {return static_cast<unsigned long>(value->ToInteger(isolate->GetCurrentContext()).ToLocalChecked()->Value());});

CAST_TO_NATIVE(int, {return static_cast<int>(value->ToInteger(isolate->GetCurrentContext()).ToLocalChecked()->Value());});
CAST_TO_NATIVE(unsigned int, {return static_cast<unsigned int>(value->ToInteger(isolate->GetCurrentContext()).ToLocalChecked()->Value());});

CAST_TO_NATIVE(short, {return static_cast<short>(value->ToInteger(isolate->GetCurrentContext()).ToLocalChecked()->Value());});
CAST_TO_NATIVE(unsigned short, {return static_cast<unsigned short>(value->ToInteger(isolate->GetCurrentContext()).ToLocalChecked()->Value());});

CAST_TO_NATIVE(char, {return static_cast<char>(value->ToInteger(isolate->GetCurrentContext()).ToLocalChecked()->Value());});
CAST_TO_NATIVE(unsigned char, {return static_cast<unsigned char>(value->ToInteger(isolate->GetCurrentContext()).ToLocalChecked()->Value());});

CAST_TO_NATIVE(wchar_t, {return static_cast<wchar_t>(value->ToInteger(isolate->GetCurrentContext()).ToLocalChecked()->Value());});
CAST_TO_NATIVE(char16_t, {return static_cast<char16_t>(value->ToInteger(isolate->GetCurrentContext()).ToLocalChecked()->Value());});

CAST_TO_NATIVE(char32_t, {return static_cast<char32_t>(value->ToInteger(isolate->GetCurrentContext()).ToLocalChecked()->Value());});




template<class Behavior, class... Ts, std::size_t... Is>
std::tuple<Ts...> cast_to_native_tuple_helper(v8::Isolate *isolate, v8::Local<v8::Array> array, std::tuple<Ts...>, std::index_sequence<Is...>) {
    return std::tuple<Ts...>(CastToNative<Ts, Behavior>()(isolate, array->Get(Is))...);
}

template<class... Ts, class Behavior>
struct CastToNative<std::tuple<Ts...>, Behavior>
{
std::tuple<Ts...> operator()(v8::Isolate *isolate, v8::Local<v8::Value> value) const {
    if (!value->IsArray()) {
        throw v8toolkit::CastException(fmt::format("CastToNative tried to create a {} object but was not given a JavaScript array", xl::demangle<std::tuple<Ts...>>()));
    }
    v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(value);

    return cast_to_native_tuple_helper<Behavior>(isolate, array, std::tuple<Ts...>(), std::index_sequence_for<Ts...>());
}
static constexpr bool callable(){return true;}


};



// A T const & can take an rvalue, so send it one, since an actual object isn't available for non-wrapped types
template<class T, typename Behavior>
struct CastToNative<T const &, Behavior, std::enable_if_t<!is_wrapped_type_v<T>>> {
T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
    return Behavior().template operator()<T const>(value);
}
static constexpr bool callable(){return true;}

};

// A T && can take an rvalue, so send it one, since a previously existing object isn't available for non-wrapped types
template<class T, typename Behavior>
struct CastToNative<T &&, Behavior, std::enable_if_t<!is_wrapped_type_v<T>>> {
T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
    return Behavior().template operator()<T const>(value);
}
static constexpr bool callable(){return true;}

};





template<typename Behavior, template<typename, typename> class ContainerTemplate, typename FirstT, typename SecondT>
ContainerTemplate<FirstT, SecondT> pair_type_helper(v8::Isolate * isolate, v8::Local<v8::Value> value) {
    if (value->IsArray()) {
        auto length = get_array_length(isolate, value);
        if (length != 2) {
            auto error = fmt::format("Array to pair must be length 2, but was {}", length);
            isolate->ThrowException(v8::String::NewFromUtf8(isolate, error.c_str()));
            throw v8toolkit::CastException(error);
        }
        auto context = isolate->GetCurrentContext();
        auto array = get_value_as<v8::Array>(isolate, value);
        auto first = array->Get(context, 0).ToLocalChecked();
        auto second = array->Get(context, 1).ToLocalChecked();
        return ContainerTemplate<FirstT, SecondT>(
            Behavior().template operator()<FirstT>(first),
            Behavior().template operator()<SecondT>(second));

    } else {
        auto error = fmt::format("CastToNative<XXX::pair<T>> requires an array but instead got %s\n", stringify_value(value));
        std::cout << error << std::endl;
        throw v8toolkit::CastException(error);
    }
}


template<typename T, typename Behavior>
struct CastToNative<T, Behavior, std::enable_if_t<
    xl::is_template_for_v<std::pair, T>
>>{
    using FirstT = typename T::first_type;
    using SecondT = typename T::second_type;

    T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return pair_type_helper<Behavior, std::pair, FirstT, SecondT>(isolate, value);
    }
};


CAST_TO_NATIVE(float, {return static_cast<float>(value->ToNumber(isolate->GetCurrentContext()).ToLocalChecked()->Value());});
CAST_TO_NATIVE(double, {return static_cast<double>(value->ToNumber(isolate->GetCurrentContext()).ToLocalChecked()->Value());});
CAST_TO_NATIVE(long double, {return static_cast<long double>(value->ToNumber(isolate->GetCurrentContext()).ToLocalChecked()->Value());});



template<typename Behavior>
struct CastToNative<v8::Local<v8::Function>, Behavior> {
v8::Local<v8::Function> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
    if(value->IsFunction()) {
        return v8::Local<v8::Function>::Cast(value);
    } else {
        throw CastException(fmt::format(
            "CastToNative<v8::Local<v8::Function>> requires a javascript function but instead got '{}'",
            stringify_value(value)));
    }
}
static constexpr bool callable(){return true;}

};


// Returns a std::unique_ptr<char[]> because a char * doesn't hold it's own memory
template<typename Behavior>
struct CastToNative<char *, Behavior> {
    std::unique_ptr<char[]> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return std::unique_ptr<char[]>(strdup(*v8::String::Utf8Value(isolate, value)));
    }
    static constexpr bool callable(){return true;}

};

// Returns a std::unique_ptr<char[]> because a char const * doesn't hold it's own memory
template<typename Behavior>
struct CastToNative<const char *, Behavior> {
    std::unique_ptr<char[]>  operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {

        return Behavior().template operator()<char *>(value);
    }
    static constexpr bool callable(){return true;}
};

// Returns a std::unique_ptr<char[]> because a string_view doesn't hold it's own memory
template<class CharT, class Traits, typename Behavior>
struct CastToNative<std::basic_string_view<CharT, Traits>, Behavior> {
    std::unique_ptr<char[]>  operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {

        return Behavior().template operator()<char *>(value);
    }
    static constexpr bool callable(){return true;}
};



template<class CharT, class Traits, class Allocator, typename Behavior>
struct CastToNative<std::basic_string<CharT, Traits, Allocator>, Behavior> {
    std::string operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
//        std::cerr << fmt::format("in cast to native string:") << std::endl;
//        print_v8_value_details(value);

        if (value->IsSymbol()) {
            return std::string(*v8::String::Utf8Value(isolate, v8::Local<v8::Symbol>::Cast(value)->Name()));
        } else {
            return std::string(*v8::String::Utf8Value(isolate, value));
        }
    }
    static constexpr bool callable(){return true;}
};



template<typename Behavior, template<class,class...> class VectorTemplate, class T, class... Rest>
auto vector_type_helper(v8::Isolate * isolate, v8::Local<v8::Value> value) {
    
    using ValueType = std::remove_reference_t<
        decltype(Behavior{}.template operator()<T>(v8::Local<v8::Value>{}))
    >;
    static_assert(!std::is_reference<ValueType>::value, "vector-like value type cannot be reference");
    using ResultType = VectorTemplate<ValueType, Rest...>;

    auto context = isolate->GetCurrentContext();
    ResultType v;
    if (value->IsArray()) {
        auto array = v8::Local<v8::Object>::Cast(value);
        auto array_length = get_array_length(isolate, array);
        for (int i = 0; i < array_length; i++) {
            auto value = array->Get(context, i).ToLocalChecked();
            v.emplace_back(std::forward<T>(Behavior().template operator()<ValueType>(value)));
        }
    } else {
        throw CastException(fmt::format("CastToNative<std::vector-like<{}>> requires an array but instead got JS: '{}'",
                                        xl::demangle<T>(),
                                        stringify_value(value)));
    }
    return v;
}


template<typename Behavior, template<class,class...> class SetTemplate, class T, class... Rest>
auto set_type_helper(v8::Isolate * isolate, v8::Local<v8::Value> value) ->
SetTemplate<std::remove_reference_t<std::result_of_t<Behavior(v8::Local<v8::Value>)>>, Rest...>
{
    using ValueType = std::remove_reference_t<
        decltype(Behavior::template operator()<T>(v8::Local<v8::Value>{}))
    >;
    static_assert(!std::is_reference<ValueType>::value, "Set-like value type cannot be reference");
    using ResultType = SetTemplate<ValueType, Rest...>;

    auto context = isolate->GetCurrentContext();
    ResultType set;
    if (value->IsArray()) {
        auto array = v8::Local<v8::Object>::Cast(value);
        auto array_length = get_array_length(isolate, array);
        for (int i = 0; i < array_length; i++) {
            auto value = array->Get(context, i).ToLocalChecked();
            set.emplace(std::forward<T>(Behavior().template operator()<ValueType>(value)));
        }
    } else {
        throw CastException(fmt::format("CastToNative<std::vector-like<{}>> requires an array but instead got JS: '{}'",
                                        xl::demangle<T>(),
                                        stringify_value(value)));
    }
    return set;
}



//Returns a vector of the requested type unless CastToNative on ElementType returns a different type, such as for char*, const char *
// Must make copies of all the values
template<class T, typename Behavior>
struct CastToNative<T, Behavior, std::enable_if_t<acts_like_array_v<T> && std::is_copy_constructible<T>::value>> {

    using NonConstT = std::remove_const_t<T>;

    T operator()(v8::Isolate *isolate, v8::Local<v8::Value> value) const {
        using ValueT = typename T::value_type;
        static_assert(!std::is_reference<ValueT>::value, "vector-like value type cannot be reference");

        auto context = isolate->GetCurrentContext();
        NonConstT a;
        if (value->IsArray()) {
            auto array = v8::Local<v8::Object>::Cast(value);
            auto array_length = get_array_length(isolate, array);
            auto back_inserter = std::back_inserter(a);

            for (int i = 0; i < array_length; i++) {
                auto value = array->Get(context, i).ToLocalChecked();
                back_inserter = Behavior().template operator()<ValueT>(value);
            }
        } else {
            throw CastException(fmt::format("CastToNative<std::vector-like<{}>> requires an array but instead got JS: '{}'",
                                            xl::demangle<T>(),
                                            stringify_value(value)));
        }
        return a;
    }
    static constexpr bool callable(){return true;}

};

// can move the elements if the underlying JS objects own their memory or can do copies if copyable, othewrise throws
// SFINAE on this is required for disambiguation, even though it can't ever catch anything
template<class T, class... Rest, typename Behavior>
struct CastToNative<std::vector<T, Rest...> &&, Behavior, std::enable_if_t<!is_wrapped_type_v<std::vector<T, Rest...>>>> {
    
    using ResultType = std::vector<std::remove_reference_t<
        decltype(Behavior::template operator()<T>(v8::Local<v8::Value>{}))
    >, Rest...>;
    
    ResultType operator()(v8::Isolate *isolate, v8::Local<v8::Value> value) const {
        return vector_type_helper<Behavior, std::vector, std::add_rvalue_reference_t<T>, Rest...>(isolate, value);
    }
    static constexpr bool callable(){return true;}
};


template<class T, typename Behavior>
struct CastToNative<T, Behavior, std::enable_if_t<acts_like_set_v<T> && !is_wrapped_type_v<T>>> {

    using NonConstT = std::remove_const_t<T>;

    T operator()(v8::Isolate *isolate, v8::Local<v8::Value> value) const {
        using ValueT = typename T::value_type;
        static_assert(!std::is_reference<ValueT>::value, "Set-like value type cannot be reference");

        auto context = isolate->GetCurrentContext();
        NonConstT set;
        if (value->IsArray()) {
            auto array = v8::Local<v8::Object>::Cast(value);
            auto array_length = get_array_length(isolate, array);
            for (int i = 0; i < array_length; i++) {
                auto value = array->Get(context, i).ToLocalChecked();
                set.emplace(Behavior().template operator()<ValueT>(value));
            }
        } else {
            throw CastException(
                fmt::format("CastToNative<std::set-like<{}>> requires an array but instead got JS: '{}'",
                            xl::demangle<ValueT>(),
                            stringify_value(value)));
        }
        return set;
    }
    static constexpr bool callable(){return true;}

};







// Cast a copyable, standard type to a unique_ptr
template<class T, class... Rest, typename Behavior>
struct CastToNative<std::unique_ptr<T, Rest...>, Behavior, 
    std::enable_if_t<
        std::is_copy_constructible<T>::value &&
        !is_wrapped_type_v<T> 
    >
>
{
    std::unique_ptr<T, Rest...> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return std::unique_ptr<T, Rest...>(new T(Behavior().template operator()<T>(value)));
    }
    static constexpr bool callable(){return true;}

};


template<class T, typename Behavior>
struct CastToNative<T, Behavior, std::enable_if_t<xl::is_template_for_v<v8::Local, T>>> {
    using NoRefT = std::remove_reference_t<T>;
    NoRefT operator()(v8::Isolate * isolate, NoRefT value) const {
        return value;
    }
    static constexpr bool callable(){return true;}
};


template<class T, typename Behavior>
struct CastToNative<T, Behavior, std::enable_if_t<xl::is_template_for_v<v8::Global, T>>> {
    using NoRefT = std::remove_reference_t<T>;
    NoRefT operator()(v8::Isolate * isolate, v8::Local<v8::Value> const & value) const {
        return NoRefT(isolate, value);
    }
    static constexpr bool callable(){return true;}
};




template<typename Behavior, template<typename...> class ContainerTemplate, typename Key, typename Value, typename... Rest>
ContainerTemplate<Key, Value, Rest...> map_type_helper(v8::Isolate * isolate, v8::Local<v8::Value> value) {

    //    MapType operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
    if (!value->IsObject()) {
        throw CastException(
            fmt::format("Javascript Object type must be passed in to convert to std::map - instead got {}",
                        stringify_value(value)));
    }

    auto context = isolate->GetCurrentContext();
    
    using ResultKey = decltype(Behavior().template operator()<Key>(std::declval<v8::Local<v8::Value>>()));
    using ResultValue = decltype(Behavior().template operator()<Value>(std::declval<v8::Local<v8::Value>>()));

    ContainerTemplate<ResultKey, ResultValue, Rest...> results;
    for_each_own_property(context, value->ToObject(isolate->GetCurrentContext()).ToLocalChecked(),
                          [isolate, &results](v8::Local<v8::Value> key, v8::Local<v8::Value> value) {
                              results.emplace(Behavior().template operator()<ResultKey>(key),
                                              Behavior().template operator()<ResultValue>(value));
                          });
    return results;
}

template<class T, typename Behavior>
struct CastToNative<T, Behavior, std::enable_if_t<acts_like_map_v<T>>> {
    using NonConstT = std::remove_const_t<T>;
    using KeyT = typename T::key_type;
    using ValueT = typename T::mapped_type;

    T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        if (!value->IsObject()) {
            throw CastException(
                fmt::format("Javascript Object type must be passed in to convert to std::map - instead got {}",
                            stringify_value(value)));
        }

        auto context = isolate->GetCurrentContext();

        NonConstT results;

        if (value->IsMap()) {
            v8::Local<v8::Map> map = v8::Local<v8::Map>::Cast(value);
            auto map_data_array = map->AsArray();

            for(int i = 0; i < map_data_array->Length() / 2; i++) {
                auto key = map_data_array->Get(i * 2);
                auto sub_value = map_data_array->Get(i * 2 + 1);

                v8toolkit::for_each_value(sub_value, [&](v8::Local<v8::Value> array_value) {

                    results.emplace(Behavior().template operator()<KeyT>(key),
                                    Behavior().template operator()<ValueT>(array_value));
                });
            }

        } else {
            for_each_own_property(context, value->ToObject(isolate->GetCurrentContext()).ToLocalChecked(),
                                  [&](v8::Local<v8::Value> key, v8::Local<v8::Value> value) {
                                      v8toolkit::for_each_value(value, [&](v8::Local<v8::Value> sub_value) {
                                          results.emplace(Behavior().template operator()<KeyT>(key),
                                                          Behavior().template operator()<ValueT>(sub_value));
                                      });
                                  });
        }
        return results;

    }
    static constexpr bool callable(){return true;}

};

template<typename Behavior, template<class,class,class...> class ContainerTemplate, class Key, class Value, class... Rest>
ContainerTemplate<Key, Value, Rest...> 
    multimap_type_helper(v8::Isolate * isolate, v8::Local<v8::Value> value) {

    if (!value->IsObject()) {
        throw CastException(
            fmt::format("Javascript Object type must be passed in to convert to std::multimap - instead got {}",
                        stringify_value(value)));
    }

    auto context = isolate->GetCurrentContext();

    ContainerTemplate<Key, Value, Rest...> results;
    for_each_own_property(context, value->ToObject(isolate->GetCurrentContext()).ToLocalChecked(),
                          [&](v8::Local<v8::Value> key, v8::Local<v8::Value> value) {
                              v8toolkit::for_each_value(value, [&](v8::Local<v8::Value> sub_value){
                                  results.emplace(Behavior().template operator()<Key>(key),
                                                  Behavior().template operator()<Value>(sub_value));
                              });
                          });
    return results;
}


//
//template<class Key, class Value, class... Args>
//struct CastToNative<std::multimap<Key, Value, Args...>> {
//
//    using ResultType = std::multimap<Key, Value, Args...>;
//
//    ResultType operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
//        return multimap_type_helper<std::multimap, Key, Value, Args...>(isolate, value);
//    }
//    static constexpr bool callable(){return true;}
//
//};




template<class ReturnT, class... Args, typename Behavior>
struct CastToNative<std::function<ReturnT(Args...)>, Behavior> {
    std::function<ReturnT(Args...)> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        auto js_function = v8toolkit::get_value_as<v8::Function>(isolate, value);

        // v8::Global's aren't copyable, but shared pointers to them are. std::functions need everything in them to be copyable
        auto context = isolate->GetCurrentContext();
        auto shared_global_function = std::make_shared<v8::Global<v8::Function>>(isolate, js_function);
        auto shared_global_context = std::make_shared<v8::Global<v8::Context>>(isolate, context);

        return [isolate, shared_global_function, shared_global_context](Args... args) -> ReturnT {
            v8::Locker locker(isolate);
            v8::HandleScope sc(isolate);
            auto context = shared_global_context->Get(isolate);
            return v8toolkit::scoped_run(isolate, context, [&]() -> ReturnT {
                assert(!context.IsEmpty());
                auto result = v8toolkit::call_javascript_function(context,
                                                                  shared_global_function->Get(isolate),
                                                                  context->Global(),
                                                                  std::tuple<Args...>(args...));
                return Behavior().template operator()<ReturnT>(result);
            });
        };
    }
};

template<class ReturnT, class... Args, typename Behavior>
struct CastToNative<func::function<ReturnT(Args...)>, Behavior> {
    std::function<ReturnT(Args...)> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        auto js_function = v8toolkit::get_value_as<v8::Function>(isolate, value);

        // v8::Global's aren't copyable, but shared pointers to them are. std::functions need everything in them to be copyable
        auto context = isolate->GetCurrentContext();
        auto shared_global_function = std::make_shared<v8::Global<v8::Function>>(isolate, js_function);
        auto shared_global_context = std::make_shared<v8::Global<v8::Context>>(isolate, context);

        return [isolate, shared_global_function, shared_global_context](Args... args) -> ReturnT {
            v8::Locker locker(isolate);
            v8::HandleScope sc(isolate);
            auto context = shared_global_context->Get(isolate);
            return v8toolkit::scoped_run(isolate, context, [&]() -> ReturnT {
                assert(!context.IsEmpty());
                auto result = v8toolkit::call_javascript_function(context,
                                                                  shared_global_function->Get(isolate),
                                                                  context->Global(),
                                                                  std::tuple<Args...>(args...));
                return Behavior().template operator()<ReturnT>(result);
            });
        };
    }
};


template <typename T, typename Behavior>
struct CastToNative<T, Behavior, std::enable_if_t<xl::is_template_for_v<std::optional, T>>> {
    using value_type = typename T::value_type;
    T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) {
        if (value->IsUndefined()) {
            return {};
        } else {
            return Behavior().template operator()<value_type>(value);
        }
    }
};


template<typename T, typename Behavior>
struct CastToNative<T, Behavior, std::enable_if_t<xl::is_template_for_v<std::chrono::duration, T>>> {
    using DurationRepresentation = typename T::rep;
    T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) {
        // if it's a nu mber, assume it's in seconds
        if (value->IsNumber()) {
            return T(Behavior().template operator()<DurationRepresentation>(value) / DurationRepresentation(1000));
        }
        // if it's an object, assume it's a
        else if (value->IsObject()) {
            v8::Local<v8::Date> date = v8::Local<v8::Date>::Cast(value);
            return T(DurationRepresentation(date->ValueOf() / DurationRepresentation(1000)));
        } else {
            throw CastException("CastToNative chrono::duration must have either a number or a JavaScript Date object");
        }
    }
};


} // end v8toolkit namespace
