

#include "v8toolkit/v8_class_wrapper.h"
#include "v8toolkit/javascript.h"
#include "v8toolkit/wrapped_class_base.h"
#include "testing.h"
#include "../class_parser/class_parser.h"


void* operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return malloc(size);
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return malloc(size);
}


using namespace v8toolkit;
using std::unique_ptr;
using std::make_unique;


class CopyableWrappedClass : public WrappedClassBase {
public:
    CopyableWrappedClass(){}
    CopyableWrappedClass(CopyableWrappedClass const &) = default;

};

bool takes_holder_called = false;
bool takes_this_called = false;
bool takes_and_returns_enum_called = false;
bool takes_isolate_and_int_called = false;
bool returns_wrapped_class_lvalue_called = false;

enum class TestEnum {TEST_ENUM_A, TEST_ENUM_B, TEST_ENUM_C};


class WrappedClass : public WrappedClassBase {
public:
    friend struct v8toolkit::WrapperBuilder<WrappedClass>;

private:
    struct Impl;
    std::unique_ptr<Impl> impl = make_unique<Impl>();

public:

    constexpr static auto ImplPointer = &WrappedClass::impl;

    // class is not default constructible
    WrappedClass(int i) : constructor_i(i) {};
    WrappedClass(WrappedClass &&) = default;
    WrappedClass(WrappedClass const &) = delete;
    WrappedClass(v8::FunctionCallbackInfo<v8::Value> const & info) {
//        std::cerr << fmt::format("IN CALLBACK INFO CONSTRUCTOR") << std::endl;
        if (info.Length() == 1 && info[0]->IsNumber()) {
            this->constructor_i = CastToNative<int>()(info.GetIsolate(), info[0]);
//            std::cerr << fmt::format("set constructor_i = {}", this->constructor_i) << std::endl;
        } else {
            throw CastException("Invalid parameters to WrappedClass constructor taking FunctionCallbackInfo object");
        }
    }
    virtual ~WrappedClass(){}

    int constructor_i;
    int i = 5;
    int ci = 5;
    std::unique_ptr<float> upf = std::make_unique<float>(3.5);
    std::unique_ptr<float> cupf = std::make_unique<float>(4.5);

    std::unique_ptr<WrappedClass> unique_ptr_wrapped_class;

    std::string string = "string value";

    int takes_int_5(int x) {
        EXPECT_EQ(x, 5);
        return x;
    }
    int takes_const_int_6(int const x) const {
        EXPECT_EQ(x, 6);
        return x;
    }
    
    
    void takes_number(int) const;
    void takes_number(double) const;

    void takes_wrapped_class(WrappedClass const & wc) const {
//        std::cerr << fmt::format("in takes wrapped class") << std::endl;
    }

    WrappedClass returns_uncopyable_type_by_value() {
        return WrappedClass{4};
    }

    std::vector<int *> returns_vector_of_ints() {
        return {};
    };

    bool default_parameters_called = false;
    void default_parameters(int j = 1,
                            char const * s = "asdf",
                            vector<std::string> && = {},
                            CopyableWrappedClass = {},
                            CopyableWrappedClass && = {},
                            CopyableWrappedClass * = nullptr){
        EXPECT_EQ(j, 1);
        EXPECT_STREQ(s, "asdf");
        this->default_parameters_called = true;
    };

    WrappedClass & returns_wrapped_class_lvalue() {
        returns_wrapped_class_lvalue_called = true;
        return *this;
    }

    static std::string static_method(int i = 5, char const * str = "asdf") {
        EXPECT_EQ(i, 5);
        EXPECT_STREQ(str, "asdf");
        return "static_method";}

    CopyableWrappedClass copyable_wrapped_class;
    std::unique_ptr<WrappedClass> up_wrapped_class;

    WrappedClass const &
    returns_const_ref_to_own_type() {
        static WrappedClass static_wrapped_class_object(5);
        return static_wrapped_class_object;
    }

    void takes_const_wrapped_ref(WrappedClass const &) {}
    bool takes_const_unwrapped_ref(std::string_view const & name) { return false; }


    static void takes_isolate_and_int(v8::Isolate * isolate, int i, v8::Local<v8::Value> value, int j) {
        EXPECT_EQ(i, 1);
        EXPECT_EQ(v8toolkit::CastToNative<int>()(isolate, value), 2);
        EXPECT_EQ(j, 3);
        takes_isolate_and_int_called = true;
    }

    void takes_this(v8::Isolate * isolate, v8toolkit::This this_object) {
        EXPECT_EQ(V8ClassWrapper<WrappedClass>::get_instance(isolate).get_cpp_object(this_object), nullptr);
        takes_this_called = true;
    }

    TestEnum takes_and_returns_enum(TestEnum test_enum) {
        EXPECT_EQ(test_enum, TestEnum::TEST_ENUM_C);
        takes_and_returns_enum_called = true;
        return TestEnum::TEST_ENUM_B;
    }


    enum class EnumClass{enum1, enum2, enum3};

    void function_takes_enum(EnumClass enum_value);

    std::unique_ptr<WrappedClass> returns_unique_ptr_wrapped_class() {
        return std::unique_ptr<WrappedClass>();
    }

    static inline int static_int = 1;
    
    
    void f1() {}
    void f2() const {}
    void f3() volatile {}
    void f4() const volatile {}

    void f1_lv() & {}
    void f2_lv() const & {}
    void f3_lv() volatile & {}
    void f4_lv() const volatile & {}

    void f1_rv() && {}
    void f2_rv() const && {}
    void f3_rv() volatile && {}
    void f4_rv() const volatile && {}

    void f1_noexcept() noexcept {}
    void f2_noexcept() const noexcept {}
    void f3_noexcept() volatile noexcept {}
    void f4_noexcept() const volatile noexcept {}
};


struct WrappedClass::Impl {
    int i = 4;
};

class WrappedClassChild : public WrappedClass {
public:
    WrappedClassChild() : WrappedClass(0) {}
};


class WrappedString : public WrappedClassBase {
public:
    std::string string;
    WrappedString(std::string string) : string(string) {}
};

namespace v8toolkit {
CAST_TO_NATIVE(WrappedString, {
    return WrappedString(CastToNative<std::string>()(isolate, value));
});

}




namespace v8toolkit {
    template<>
    struct WrapperBuilder<WrappedClass> {
        void operator()(v8toolkit::IsolatePtr i){
            auto & w = V8ClassWrapper<WrappedClass>::get_instance(*i);
            auto & w_v = V8ClassWrapper<WrappedClass volatile>::get_instance(*i);
            w.add_member<&WrappedClass::i>("i");
            w.add_member<&WrappedClass::constructor_i>("constructor_i");
            w.add_member<&WrappedClass::ci>("ci");
            w.add_member<&WrappedClass::upf>("upf");
            w.add_member<&WrappedClass::cupf>("cupf");
            w.add_member<&WrappedClass::string>("string");
            w.add_member<&WrappedClass::copyable_wrapped_class>("copyable_wrapped_class");
            w.add_member<&WrappedClass::up_wrapped_class>("up_wrapped_class");
            w.add_method("takes_int_5", &WrappedClass::takes_int_5);
//            w.add_method("takes_number", &WrappedClass::takes_number);

            w.add_method("fake_method", [](WrappedClass * wc){return wc->constructor_i;});
            w.add_method("const_fake_method", [](WrappedClass const * wc, int i, bool b){if (b){return wc->constructor_i;} else {return 0;}}, std::tuple<bool>(false));

            w.add_method("takes_const_int_6", &WrappedClass::takes_const_int_6);
            w.add_method("takes_wrapped_class", &WrappedClass::takes_wrapped_class);
            w.add_method("takes_const_wrapped_ref", &WrappedClass::takes_const_wrapped_ref);
            w.add_static_method("takes_isolate_and_int", &WrappedClass::takes_isolate_and_int, std::tuple<int>(3));
            w.add_method("takes_this", &WrappedClass::takes_this);

            w.add_method("returns_wrapped_class_lvalue", &WrappedClass::returns_wrapped_class_lvalue);
            w.add_method("takes_const_unwrapped_ref", &WrappedClass::takes_const_unwrapped_ref);
            w.add_method("returns_uncopyable_type_by_value", &WrappedClass::returns_uncopyable_type_by_value);
            w.add_method("returns_unique_ptr_wrapped_class", &WrappedClass::returns_unique_ptr_wrapped_class);
            w.add_method("returns_const_ref_to_own_type", &WrappedClass::returns_const_ref_to_own_type);
            w.add_method("returns_vector_of_ints", &WrappedClass::returns_vector_of_ints);
            w.add_method("default_parameters", &WrappedClass::default_parameters,
                         std::tuple<
                             int,
                             char const *,
                             vector<std::string>,
                             CopyableWrappedClass,
                             CopyableWrappedClass,
                             CopyableWrappedClass*>(1, "asdf", {}, {}, {}, nullptr));
            w.add_method("takes_and_returns_enum", &WrappedClass::takes_and_returns_enum);
            w.add_member<&WrappedClass::unique_ptr_wrapped_class>("unique_ptr_wrapped_class");
            w.add_static_method("static_method", &WrappedClass::static_method, std::make_tuple(5, "asdf"));
            w.add_static_method("inline_static_method", [](int i){
                EXPECT_EQ(i, 7);
            }, std::tuple<int>(7));
            w.add_member<WrappedClass::ImplPointer, &WrappedClass::Impl::i>("pimpl_i");
            w.add_member_readonly<WrappedClass::ImplPointer, &WrappedClass::Impl::i>("pimpl_i_readonly");
            w.add_static_member("static_int", &WrappedClass::static_int);
            w.add_enum("enum_test", {{"A", 1}, {"B", 2}, {"C", 3}});

            w.add("f1", &WrappedClass::f1);
            w.add("f2", &WrappedClass::f2);

            w_v.add("f3", &WrappedClass::f3);
            w_v.add("f4", &WrappedClass::f4);
            w.add("f1_lv", &WrappedClass::f1_lv);
            w.add("f2_lv", &WrappedClass::f2_lv);
            w_v.add("f3_lv", &WrappedClass::f3_lv);
            w_v.add("f4_lv", &WrappedClass::f4_lv);
//            w.add(&WrappedClass::f1_rv);
//            w.add(&WrappedClass::f2_rv);
//            w.add(&WrappedClass::f3_rv);
//            w.add(&WrappedClass::f4_rv);
            w.add("f1_noexcept", &WrappedClass::f1_noexcept);
            w.add("f2_noexcept", &WrappedClass::f2_noexcept);
            w_v.add("f3_noexcept", &WrappedClass::f3_noexcept);
            w_v.add("f4_noexcept", &WrappedClass::f4_noexcept);
            
            
            w.set_compatible_types<WrappedClassChild>();
            w.finalize(true);
            w_v.finalize(true);
            w.add_constructor<int>("WrappedClass", *i);
        }
    };

}


class WrappedClassFixture : public JavaScriptFixture {
public:
    WrappedClassFixture() {
        ISOLATE_SCOPED_RUN(*i);
        {
           WrapperBuilder<WrappedClass>()(i);
        }
        {
            auto & w = V8ClassWrapper<WrappedClassChild>::get_instance(*i);
            w.set_parent_type<WrappedClass>();
            w.finalize(true);
            w.add_constructor("WrappedClassChild", *i);
        }
        {
            auto & w = V8ClassWrapper<CopyableWrappedClass>::get_instance(*i);
            w.finalize();
            w.add_constructor<>("CopyableWrappedClass", *i);
        }

        {
            auto & w = V8ClassWrapper<WrappedString>::get_instance(*i);
            w.add_member<&WrappedString::string>("string");
            w.finalize();
            w.add_constructor<std::string const &>("WrappedString", *i);
        }



        i->add_function("takes_wrapped_class_lvalue", [](WrappedClass & wrapped_class){
            EXPECT_EQ(wrapped_class.string, "string value");
            return wrapped_class.i;
        });

        i->add_function("takes_wrapped_class_rvalue", [](WrappedClass && wrapped_class){
            EXPECT_EQ(wrapped_class.string, "string value");
            WrappedClass wc(std::move(wrapped_class));
            EXPECT_EQ(wc.string, "string value");
            EXPECT_EQ(wrapped_class.string, "");

            return wrapped_class.i;
        });

        i->add_function("takes_wrapped_class_unique_ptr", [](std::unique_ptr<WrappedClass> wrapped_class){
            return wrapped_class->i;
        });

        i->add_function("returns_wrapped_class_lvalue_ref", []()->WrappedClass&{

            static WrappedClass static_wrapped_class(1);
            return static_wrapped_class;
        });

        create_context();

    }
};



TEST_F(WrappedClassFixture, Accessors) {


    (*c)([&] {
        {
            c->run("EXPECT_TRUE(new WrappedClass(1).i == 5)");
            c->run("EXPECT_TRUE(new WrappedClass(2).ci == 5)");

            c->run("EXPECT_TRUE(new WrappedClass(3).upf == 3.5)");
            c->run("EXPECT_TRUE(new WrappedClass(4).cupf == 4.5)");

            c->run("{let wc = new WrappedClass(\"5\");  EXPECT_TRUE(wc.constructor_i == 5)}");

            c->run("{let wc = new WrappedClass(\"5\");  wc.takes_wrapped_class(1);}");
        }
    });
}

TEST_F(WrappedClassFixture, SimpleFunctions) {

    (*c)([&] {
        {
            c->run("EXPECT_TRUE(new WrappedClass(5).takes_int_5(5) == 5)");
            c->run("EXPECT_TRUE(new WrappedClass(6).takes_const_int_6(6) == 6)");

            c->run("EXPECT_TRUE(WrappedClass.static_method() == `static_method`)");
            c->run("EXPECT_TRUE(new WrappedClass(5).fake_method() == 5)");
            c->run("EXPECT_TRUE(new WrappedClass(6).const_fake_method(1, true) == 6)");
            c->run("EXPECT_TRUE(new WrappedClass(6).const_fake_method(1, false) == 0)");
            c->run("EXPECT_TRUE(new WrappedClass(6).const_fake_method(1) == 0)");
            c->run("EXPECT_TRUE(new WrappedClass(7).takes_and_returns_enum(2) == 1)");
            EXPECT_TRUE(takes_and_returns_enum_called);
            takes_and_returns_enum_called = false; // reset for any future use
        }
    });
}

TEST_F(WrappedClassFixture, Enumerations) {
    (*c)([&] {
        {
            c->run("EXPECT_TRUE(new WrappedClass(5).enum_test.A == 1)");
            c->run("EXPECT_TRUE(new WrappedClass(5).enum_test.B == 2)");
            c->run("EXPECT_TRUE(new WrappedClass(5).enum_test.C == 3)");
        }
    });

}



TEST_F(WrappedClassFixture, CallingWithLvalueWrappedClass) {

    (*c)([&] {
        {
            // calling with owning object
            auto result = c->run(
                "wc = new WrappedClass(7); takes_wrapped_class_lvalue(wc);"
                    "EXPECT_EQJS(wc.string, `string value`); wc;"
            );
            EXPECT_TRUE(V8ClassWrapper<WrappedClass>::does_object_own_memory(result.Get(*i)->ToObject()));
        }
        {

            // calling with non-owning object
            c->run("non_owning_wc = returns_wrapped_class_lvalue_ref();  takes_wrapped_class_lvalue(non_owning_wc);"
                       "EXPECT_EQJS(non_owning_wc.string, `string value`);"
            );

        }
    });
}

TEST_F(WrappedClassFixture, CallingWithRvalueWrappedClass) {

    (*c)([&] {
        {
            // calling with owning object
            auto result = c->run("wc = new WrappedClass(8); takes_wrapped_class_rvalue(wc);"
                                     "EXPECT_EQJS(wc.string, ``); wc;"
            );

            // object still owns its memory, even though the contents may have been moved out of
            EXPECT_TRUE(V8ClassWrapper<WrappedClass>::does_object_own_memory(result.Get(*i)->ToObject()));
        }

        {
            // calling with non-owning object
            EXPECT_THROW(
                c->run("non_owning_wc = returns_wrapped_class_lvalue_ref(); takes_wrapped_class_rvalue(non_owning_wc);"
                           "EXPECT_EQJS(wc.string, ``);"
                ), V8Exception);
        }
    });
}



TEST_F(WrappedClassFixture, CallingWithUniquePtr) {

    (*c)([&] {
        {
            // calling with owning object
            auto result = c->run(
                "wc = new WrappedClass(9); takes_wrapped_class_unique_ptr(wc);"
                    // "EXPECT_EQJS(wc.string, ``);" <== can't do this, the memory is *GONE* not just moved out of
                "wc;"
            );
            EXPECT_FALSE(V8ClassWrapper<WrappedClass>::does_object_own_memory(result.Get(*i)->ToObject()));

        }
        {
            // call with unique_ptr when owning, then call again after first call takes ownership
            EXPECT_THROW(
                c->run(
                    "wc = new WrappedClass(10); takes_wrapped_class_unique_ptr(wc); takes_wrapped_class_unique_ptr(wc);"
                        "EXPECT_EQJS(wc.string, ``); wc;"
                ), V8Exception);
        }
        {
            // calling with non-owning object
            EXPECT_THROW(
                c->run(
                    "non_owning_wc = returns_wrapped_class_lvalue_ref(); takes_wrapped_class_unique_ptr(non_owning_wc);"

                ), V8Exception);
        }
    });
}


TEST_F(WrappedClassFixture, ReturningUniquePtr) {

    c->add_function("returns_unique_ptr", [&](){
        return std::make_unique<WrappedClass>(4);
    });


    (*c)([&](){

        c->run("returns_unique_ptr();");

    });
}


TEST_F(WrappedClassFixture, FunctionTakesCopyOfWrappedType) {
    c->add_function("wants_copy_of_wrapped_type", [&](CopyableWrappedClass){});

    (*c)([&](){
        c->run("wants_copy_of_wrapped_type(new CopyableWrappedClass());");
    });

    // cannot make copy of uncopyable type
    if constexpr(CastToNative<WrappedClass>::callable()) {
        EXPECT_TRUE(false);
    }
    // cannot make copy of uncopyable type
    bool copy_of_copyable_type = false;
    if constexpr(CastToNative<CopyableWrappedClass>::callable()) {
        copy_of_copyable_type = true;
    }
    EXPECT_TRUE(copy_of_copyable_type);
}



// If the wrapped type has a custom CastToNative, use that instead of blindly trying to get the
//   C++ object pointer out of a JavaScript value at that position
TEST_F(WrappedClassFixture, PreferSpecializedCastToNativeDuringParameterBuilder) {
    (*c)([&]() {

        c->add_function("wants_wrapped_string_rvalue_ref", [&](WrappedString && wrapped_string) {
            EXPECT_EQ(wrapped_string.string, "rvalue");
        });
        c->add_function("wants_wrapped_string", [&](WrappedString wrapped_string) {
            EXPECT_EQ(wrapped_string.string, "copy");
        });
        c->add_function("wants_wrapped_string_lvalue_ref", [&](WrappedString & wrapped_string) {
            EXPECT_EQ(wrapped_string.string, "lvalue");
        });
        c->add_function("wants_wrapped_string_const_lvalue_ref", [&](WrappedString const & wrapped_string) {
            EXPECT_EQ(wrapped_string.string, "lvalue");
        });


        c->run("wants_wrapped_string_rvalue_ref(`rvalue`);");
        c->run("wants_wrapped_string_rvalue_ref(new WrappedString(`rvalue`));");

        c->run("wants_wrapped_string(`copy`);");
        c->run("wants_wrapped_string(new WrappedString(`copy`));");

        c->run("wants_wrapped_string_lvalue_ref(`lvalue`);");
        c->run("wants_wrapped_string_lvalue_ref(new WrappedString(`lvalue`));");

        c->run("wants_wrapped_string_const_lvalue_ref(`lvalue`);");
        c->run("wants_wrapped_string_const_lvalue_ref(new WrappedString(`lvalue`));");
    });
}


TEST_F(WrappedClassFixture, DefaultParameters) {

    (*c)([&](){

        auto result = c->run("let wc = new WrappedClass(11); wc.default_parameters(); wc;");
        WrappedClass * pwc = CastToNative<WrappedClass *>()(*i, result.Get(*i));
        EXPECT_TRUE(pwc->default_parameters_called);
    });

}



TEST_F(WrappedClassFixture, DerivedTypesLValue) {
    c->add_function("wants_wrapped_class", [&](WrappedClass &) {
    });
    c->add_function("returns_wrapped_class_child", [&]()->WrappedClassChild &{
        static WrappedClassChild wcc;
        return wcc;
    });

    c->run("wants_wrapped_class(returns_wrapped_class_child());");

}



TEST_F(WrappedClassFixture, DerivedTypesPointer) {
    c->add_function("wants_wrapped_class", [&](WrappedClass *) {
    });
    c->add_function("returns_wrapped_class_child", [&]()->WrappedClassChild *{
        static WrappedClassChild wcc;
        return &wcc;
    });

    c->run("wants_wrapped_class(returns_wrapped_class_child());");

}

TEST_F(WrappedClassFixture, DerivedTypesRValue) {
    c->add_function("wants_wrapped_class", [&](WrappedClass &&) {
    });
    c->add_function("returns_wrapped_class_child", [&]()->WrappedClassChild &&{
        static WrappedClassChild wcc;
        return std::move(wcc);
    });

    c->run("wants_wrapped_class(returns_wrapped_class_child());");

}


TEST_F(WrappedClassFixture, DerivedTypesLValueRValueMismatch) {
    c->add_function("wants_wrapped_class", [&](WrappedClass &&) {
    });
    c->add_function("returns_wrapped_class_child", [&]()->WrappedClassChild &{
        static WrappedClassChild wcc;
        return wcc;
    });

    EXPECT_THROW(
        c->run("wants_wrapped_class(returns_wrapped_class_child());"),
        V8Exception
    );
}

TEST_F(WrappedClassFixture, DerivedTypesUniquePointer) {
    c->add_function("wants_wrapped_class", [&](std::unique_ptr<WrappedClass>) {
    });
    c->add_function("returns_wrapped_class_child", [&]()->std::unique_ptr<WrappedClassChild> {
        return std::make_unique<WrappedClassChild>();
    });

    c->run("wants_wrapped_class(returns_wrapped_class_child());");
}


TEST_F(WrappedClassFixture, DerivedTypesUniquePointerReverseCast) {
    c->add_function("wants_wrapped_class", [&](std::unique_ptr<WrappedClassChild>) {
    });

    // really return a wrapped class child, but call it a wrapped class instead
    c->add_function("returns_wrapped_class_child", [&]()->std::unique_ptr<WrappedClass> {
        return std::make_unique<WrappedClass>(6);
    });

    c->run("wants_wrapped_class(returns_wrapped_class_child());");
}


TEST_F(WrappedClassFixture, CastToJSRValueRef) {
    WrappedClass wc(2);
    (*c)([&]() {

        auto result = CastToJS<WrappedClass &&>()(*i, std::move(wc));
        EXPECT_TRUE(V8ClassWrapper<WrappedClass>::does_object_own_memory(result->ToObject()));
    });
}


TEST_F(WrappedClassFixture, TakesConstWrappedRef) {
    WrappedClass wc(3);
    (*c)([&]() {

        auto result = CastToJS<WrappedClass &&>()(*i, std::move(wc));
        EXPECT_TRUE(V8ClassWrapper<WrappedClass>::does_object_own_memory(result->ToObject()));
    });
}




TEST_F(WrappedClassFixture, TakesConstUnwrappedRef) {
    WrappedClass wc(4);
    (*c)([&]() {

        auto result = CastToJS<WrappedClass &&>()(*i, std::move(wc));
        EXPECT_TRUE(V8ClassWrapper<WrappedClass>::does_object_own_memory(result->ToObject()));
    });
}


// test calling
TEST_F(WrappedClassFixture, StaticMethodDefaultValue) {
    c->run("WrappedClass.static_method(5)");
    c->run("WrappedClass.static_method()");
    c->run("WrappedClass.static_method()");

    c->run("WrappedClass.inline_static_method(7);");
    c->run("WrappedClass.inline_static_method();");
    c->run("WrappedClass.inline_static_method();");

//    c->run("print(WrappedClass.static_int)");

    c->run("EXPECT_TRUE(WrappedClass.static_int == 1)");
}


TEST_F(WrappedClassFixture, FunctionTakesIsolatePointer) {
    takes_isolate_and_int_called = false;
    c->run("WrappedClass.takes_isolate_and_int(1, 2, 3);");
    EXPECT_TRUE(takes_isolate_and_int_called);

    takes_isolate_and_int_called = false;
    c->run("WrappedClass.takes_isolate_and_int(1, 2);");
    EXPECT_TRUE(takes_isolate_and_int_called);

}


TEST_F(WrappedClassFixture, WrapDerivedTypeFromBaseWrapper) {
    (*c)([&]() {

        returns_wrapped_class_lvalue_called = false;
        auto result = c->run("new WrappedClass(12).returns_wrapped_class_lvalue();");
        EXPECT_TRUE(returns_wrapped_class_lvalue_called);
        auto wrapped_class = get_value_as<WrappedClass*>(*i, result);
        EXPECT_EQ(wrapped_class->i, 5);
    });
}


TEST_F(WrappedClassFixture, FunctionTakesHolder) {
    (*c)([&]() {

        takes_this_called = false;
        c->run("var wc2 = new WrappedClass(13);wc2.base = true;"
                   "var derived_wc2 = Object.create(wc2); derived_wc2.derived = true;");

        auto base = c->run("wc2");
        auto derived = c->run("derived_wc2");



        c->run("derived_wc2.takes_this();");
        EXPECT_TRUE(takes_this_called);
    });
}




TEST_F(WrappedClassFixture, CastToNativeNonCopyableTypeByValue) {
    auto isolate = c->isolate;
    (*c)([&]() {
        auto wrapped_class = c->run("new WrappedClass(40);");
        EXPECT_EQ(CastToNative<WrappedClass>()(isolate, wrapped_class.Get(isolate)).constructor_i, 40);

        // try again, but with a non-owning javascript object
        auto wrapped_class2 = c->run("new WrappedClass(41);");
        auto & wrapper = v8toolkit::V8ClassWrapper<WrappedClass>::get_instance(isolate);
        wrapper.release_internal_field_memory(wrapped_class2.Get(isolate)->ToObject());
        EXPECT_THROW(CastToNative<WrappedClass>()(c->isolate, wrapped_class2.Get(isolate)), CastException);
    });
}

/// This was only for trying to reproduce an error that couldn't be reproduced 

//TEST_F(WrappedClassFixture, ConsistentWrappedClassVariableAddress) {
//    
//    auto isolate = c->isolate;
//    (*c)([&]() {
//        auto wc = WrappedClass(42);
//        std::cerr << fmt::format("&wc = {}\n", (void*)&wc);
//        c->add_variable("wc", c->wrap_object(&wc));
//        c->run("printobj(wc)");
//        c->run("println(\"wc.constructor_i:\", wc.constructor_i)");
//    });
//    EXPECT_TRUE(false);
//}


TEST_F(WrappedClassFixture, PimplMember) {
    (*c)([&]() {
        c->run("EXPECT_TRUE(new WrappedClass(40).pimpl_i, 4)");
    });
}

