#include "testing.h"
#include "v8toolkit/cast_to_native_impl.h"


TEST_F(JavaScriptFixture, RequireErrors) {

    this->create_context();

    (*c)([&]{
        v8::Local<v8::Value> value;
        EXPECT_FALSE(require(*c, "does_not_exist.js", value, {"."}));
        EXPECT_THROW(require(*c, "syntax_error.js", value, {"."}), V8CompilationException);
        EXPECT_THROW(require(*c, "runtime_error.js", value, {"."}), V8ExecutionException);
    });
}


TEST_F(JavaScriptFixture, MakeLocal) {
    this->create_context();

    (*c)([&]{
        {
            v8::Local<v8::Value> value = v8::Number::New(isolate, 4.0);
            auto result = get_value_as_optional<v8::Object>(value);
            EXPECT_FALSE(result);
        }
        {
            v8::Local<v8::Value> value = v8::Object::New(isolate);
            auto result = get_value_as_optional<v8::Object>(value);
            EXPECT_TRUE(result);
        }
    });
}

TEST_F(JavaScriptFixture, GetPropertyAs) {

    this->create_context();

    (*c)([&]{
        auto object = c->run("({a: 1, b: undefined})");
        EXPECT_TRUE(get_property_as(object, "a"));
        EXPECT_TRUE(get_property_as(object, "b"));
        EXPECT_FALSE(get_property_as(object, "c"));
    });

}

TEST_F(JavaScriptFixture, ReleaseRequiredModulesBeforeIsolateGoesAway) {

    this->create_context();

    (*c)([&]{
        v8::Local<v8::Value> value;
        require(*c, "valid_module.js", value, {"."});
    });

    // get the Isolate destructor to call in the body of the test instead of implicitly in fixture destructor code
    c.reset();
    i.reset();

    // NO CODE AFTER THIS
}

TEST_F(JavaScriptFixture, PropertiesOfArrays) {
    this->create_context();

    auto array = c->run("[1,2,3]");
    GLOBAL_CONTEXT_SCOPED_RUN(c->get_isolate(), c->get_global_context());
    auto properties = get_interesting_properties(c->get_context(), array.Get(c->get_isolate()).As<v8::Object>());
    EXPECT_NE(xl::find(properties, "forEach"s), properties.end());
}

TEST_F(JavaScriptFixture, DateConversions) {
    this->create_context();

    {
        auto date = c->run("new Date(`january 1, 1970 03:24:01:100`) - new Date(`january 1, 1970 03:24:01`)");
        GLOBAL_CONTEXT_SCOPED_RUN(c->get_isolate(), c->get_global_context());
        EXPECT_EQ(CastToNative<std::chrono::duration<float>>()(c->get_isolate(), date.Get(c->get_isolate())).count(),
                  0.1f);
    }
    {
        auto date = c->run("100");
        GLOBAL_CONTEXT_SCOPED_RUN(c->get_isolate(), c->get_global_context());
        EXPECT_EQ(CastToNative<std::chrono::duration<float>>()(c->get_isolate(), date.Get(c->get_isolate())).count(),
                  0.1f);
    }
    {
        auto date = c->run("new Date(`january 1, 1970 03:24:01:100`) - new Date(`january 1, 1970 03:24:01`)");
        GLOBAL_CONTEXT_SCOPED_RUN(c->get_isolate(), c->get_global_context());
        EXPECT_EQ(CastToNative<std::chrono::duration<int>>()(c->get_isolate(), date.Get(c->get_isolate())).count(),
                  0);
    }
    {
        auto date = c->run("100");
        GLOBAL_CONTEXT_SCOPED_RUN(c->get_isolate(), c->get_global_context());
        EXPECT_EQ(CastToNative<std::chrono::duration<int>>()(c->get_isolate(), date.Get(c->get_isolate())).count(),
                  0);
    }
    {
        auto date = c->run("new Date(`january 1, 1970 03:24:02:100`) - new Date(`january 1, 1970 03:24:01`)");
        GLOBAL_CONTEXT_SCOPED_RUN(c->get_isolate(), c->get_global_context());
        EXPECT_EQ(CastToNative<std::chrono::duration<int>>()(c->get_isolate(), date.Get(c->get_isolate())).count(),
                  1);
    }
    {
        auto date = c->run("1100");
        GLOBAL_CONTEXT_SCOPED_RUN(c->get_isolate(), c->get_global_context());
        EXPECT_EQ(CastToNative<std::chrono::duration<int>>()(c->get_isolate(), date.Get(c->get_isolate())).count(),
                  1);
    }
}


TEST_F(JavaScriptFixture, Stringify) {
    this->create_context();

    auto big_object = c->run("{{a:{b:{c:{d:{e:{f:{g:{h:{}}}}}}}}}}");
    GLOBAL_CONTEXT_SCOPED_RUN(c->get_isolate(), c->get_global_context());
    stringify_value(big_object.Get(c->get_isolate()));
}

