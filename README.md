# v8-class-wrapper
Utilities for automatically wrapping c++ classes for use in javascript with the V8 Javascript engine - compatible with V8 v4.9.0.0 (i.e. not the original API that cvv8 (a much better library for doing this, but it doesn't work on the current V8 API)

```
class MyClass {
public: 
	MyClass(){}
	int some_method(int x){return 2*x;}
	int value;
};

auto wrapper = V8ClassWrapper<MyClass> wrapped_point("MyClass", isolate, global_templ);
wrapper.add_method(&MyClass::some_method, "someMethod").add_member(&MyClass::value, "value");
```

allows you to say in javascript:

```
var o = new MyClass();
var result = o.someMethod(5);
o.value = result;
```

It doesn't handle a lot of types, there's no good handling for overloaded c++ methods, it only uses the default c++ constructor for making methods.. 

Not sure how much more work this will get, but it's at least something good to look at, if you're reasonably good with intermediate-level c++ templating syntax.