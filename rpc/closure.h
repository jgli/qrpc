#ifndef QRPC_RPC_CLOSURE_H
#define QRPC_RPC_CLOSURE_H

/*
 * Extend the Closure of google::protobuf.
 *
 * Abstract interface for a callback.  When calling an RPC, you must provide
 * a Closure to call when the procedure completes.  See the Service interface
 * in service.h.
 *
 * To automatically construct a Closure which calls a particular function or
 * method with a particular set of parameters, use the NewCallback() function.
 * Example:
 *   void FooDone(const FooResponse* response) {
 *     ...
 *   }
 *
 *   void CallFoo() {
 *     ...
 *     // When done, call FooDone() and pass it a pointer to the response.
 *     Closure* callback = NewCallback(&FooDone, response);
 *     // Make the call.
 *     service->Foo(controller, request, response, callback);
 *   }
 *
 * Example that calls a method:
 *   class Handler {
 *    public:
 *     ...
 *
 *     void FooDone(const FooResponse* response) {
 *       ...
 *     }
 *
 *     void CallFoo() {
 *       ...
 *       // When done, call FooDone() and pass it a pointer to the response.
 *       Closure* callback = NewCallback(this, &Handler::FooDone, response);
 *       // Make the call.
 *       service->Foo(controller, request, response, callback);
 *     }
 *   };
 *
 * Currently NewCallback() supports binding zero to nine arguments.
 *
 * Callbacks created with NewCallback() automatically delete themselves when
 * executed.  They should be used when a callback is to be called exactly
 * once (usually the case with RPC callbacks).  If a callback may be called
 * a different number of times (including zero), create it with
 * NewPermanentCallback() instead.  You are then responsible for deleting the
 * callback (using the "delete" keyword as normal).
 *
 * Note that NewCallback() is a bit touchy regarding argument types.  Generally,
 * the values you provide for the parameter bindings must exactly match the
 * types accepted by the callback function.  For example:
 *   void Foo(string s);
 *   NewCallback(&Foo, "foo");          // WON'T WORK:  const char* != string
 *   NewCallback(&Foo, string("foo"));  // WORKS
 * Also note that the arguments cannot be references:
 *   void Foo(const string& s);
 *   string my_str;
 *   NewCallback(&Foo, my_str);  // WON'T WORK:  Can't use referecnes.
 * However, correctly-typed pointers will work just fine.
 */

#include <stdint.h>
#include <string>

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

namespace qrpc {

class Closure : public google::protobuf::Closure {
public:
    inline Closure() { }
    virtual ~Closure();

    virtual void Run() = 0;

private:
    /* No copying allowed */
    Closure(const Closure &);
    void operator=(const Closure &);
};

namespace internal {

class FunctionClosure0 : public Closure {
public:
    typedef void (*FunctionType)();
    
    FunctionClosure0(FunctionType function, bool self_deleting)
        : function_(function)
        , self_deleting_(self_deleting) { }
    ~FunctionClosure0() { }
    
    void Run() {
        bool needs_delete = self_deleting_;  // read in case callback deletes
        function_();
        if (needs_delete) delete this;
    }

private:
    FunctionType function_;
    bool self_deleting_;
};

template <typename Class>
class MethodClosure0 : public Closure {
public:
    typedef void (Class::*MethodType)();

    MethodClosure0(Class *object, MethodType method, bool self_deleting)
        : object_(object)
        , method_(method)
        , self_deleting_(self_deleting) { }
    ~MethodClosure0() { }

    void Run() {
        bool needs_delete = self_deleting_;
        (object_->*method_)();
        if (needs_delete) delete this;
    }

private:
    Class *object_;
    MethodType method_;
    bool self_deleting_;
};

template <typename Arg1>
class FunctionClosure1 : public Closure {
public:
    typedef void (*FunctionType)(Arg1 arg1);

    FunctionClosure1(FunctionType function, bool self_deleting,
                     Arg1 arg1)
        : function_(function)
        , self_deleting_(self_deleting)
        , arg1_(arg1) { }
    ~FunctionClosure1() { }

    void Run() {
        bool needs_delete = self_deleting_;
        function_(arg1_);
        if (needs_delete) delete this;
    }

private:
    FunctionType function_;
    bool self_deleting_;
    Arg1 arg1_;
};

template <typename Class, typename Arg1>
class MethodClosure1 : public Closure {
public:
    typedef void (Class::*MethodType)(Arg1 arg1);

    MethodClosure1(Class *object, MethodType method, bool self_deleting,
                   Arg1 arg1)
        : object_(object)
        , method_(method)
        , self_deleting_(self_deleting)
        , arg1_(arg1) { }
    ~MethodClosure1() { }

    void Run() {
        bool needs_delete = self_deleting_;
        (object_->*method_)(arg1_);
        if (needs_delete) delete this;
    }

private:
    Class *object_;
    MethodType method_;
    bool self_deleting_;
    Arg1 arg1_;
};

template <typename Arg1, typename Arg2>
class FunctionClosure2 : public Closure {
public:
    typedef void (*FunctionType)(Arg1 arg1, Arg2 arg2);

    FunctionClosure2(FunctionType function, bool self_deleting,
                     Arg1 arg1, Arg2 arg2)
        : function_(function)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2) { }
    ~FunctionClosure2() { }

    void Run() {
        bool needs_delete = self_deleting_;
        function_(arg1_, arg2_);
        if (needs_delete) delete this;
    }

private:
    FunctionType function_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
};

template <typename Class ,typename Arg1, typename Arg2>
class MethodClosure2 : public Closure {
public:
    typedef void (Class::*MethodType)(Arg1 arg1, Arg2 arg2);

    MethodClosure2(Class *object, MethodType method, bool self_deleting,
                   Arg1 arg1, Arg2 arg2)
        : object_(object)
        , method_(method)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2) { }
    ~MethodClosure2() { }

    void Run() {
        bool needs_delete = self_deleting_;
        (object_->*method_)(arg1_, arg2_);
        if (needs_delete) delete this;
    }

private:
    Class *object_;
    MethodType method_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
};

template <typename Arg1, typename Arg2, typename Arg3>
class FunctionClosure3 : public Closure {
public:
    typedef void (*FunctionType)(Arg1 arg1, Arg2 arg2, Arg3 arg3);

    FunctionClosure3(FunctionType function, bool self_deleting,
                     Arg1 arg1, Arg2 arg2, Arg3 arg3)
        : function_(function)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2)
        , arg3_(arg3) { }
    ~FunctionClosure3() { }

    void Run() {
        bool needs_delete = self_deleting_;
        function_(arg1_, arg2_, arg3_);
        if (needs_delete) delete this;
    }

private:
    FunctionType function_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
};

template <typename Class ,typename Arg1, typename Arg2, typename Arg3>
class MethodClosure3 : public Closure {
public:
    typedef void (Class::*MethodType)(Arg1 arg1, Arg2 arg2, Arg3 arg3);

    MethodClosure3(Class *object, MethodType method, bool self_deleting,
                   Arg1 arg1, Arg2 arg2, Arg3 arg3)
        : object_(object)
        , method_(method)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2)
        , arg3_(arg3) { }
    ~MethodClosure3() { }

    void Run() {
        bool needs_delete = self_deleting_;
        (object_->*method_)(arg1_, arg2_, arg3_);
        if (needs_delete) delete this;
    }

private:
    Class *object_;
    MethodType method_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
};

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
class FunctionClosure4 : public Closure {
public:
    typedef void (*FunctionType)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4);

    FunctionClosure4(FunctionType function, bool self_deleting,
                     Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
        : function_(function)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2)
        , arg3_(arg3)
        , arg4_(arg4) { }
    ~FunctionClosure4() { }

    void Run() {
        bool needs_delete = self_deleting_;
        function_(arg1_, arg2_, arg3_, arg4_);
        if (needs_delete) delete this;
    }

private:
    FunctionType function_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
    Arg4 arg4_;
};

template <typename Class ,typename Arg1, typename Arg2, typename Arg3, typename Arg4>
class MethodClosure4 : public Closure {
public:
    typedef void (Class::*MethodType)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4);

    MethodClosure4(Class *object, MethodType method, bool self_deleting,
                   Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
        : object_(object)
        , method_(method)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2)
        , arg3_(arg3)
        , arg4_(arg4) { }
    ~MethodClosure4() { }

    void Run() {
        bool needs_delete = self_deleting_;
        (object_->*method_)(arg1_, arg2_, arg3_, arg4_);
        if (needs_delete) delete this;
    }

private:
    Class *object_;
    MethodType method_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
    Arg4 arg4_;
};

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
class FunctionClosure5 : public Closure {
public:
    typedef void (*FunctionType)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5);

    FunctionClosure5(FunctionType function, bool self_deleting,
                     Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4,
                     Arg5 arg5)
        : function_(function)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2)
        , arg3_(arg3)
        , arg4_(arg4)
        , arg5_(arg5) { }
    ~FunctionClosure5() { }

    void Run() {
        bool needs_delete = self_deleting_;
        function_(arg1_, arg2_, arg3_, arg4_, arg5_);
        if (needs_delete) delete this;
    }

private:
    FunctionType function_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
    Arg4 arg4_;
    Arg5 arg5_;
};

template <typename Class ,typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
class MethodClosure5 : public Closure {
public:
    typedef void (Class::*MethodType)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5);

    MethodClosure5(Class *object, MethodType method, bool self_deleting,
                   Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4,
                   Arg5 arg5)
        : object_(object)
        , method_(method)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2)
        , arg3_(arg3)
        , arg4_(arg4)
        , arg5_(arg5) { }
    ~MethodClosure5() { }

    void Run() {
        bool needs_delete = self_deleting_;
        (object_->*method_)(arg1_, arg2_, arg3_, arg4_, arg5_);
        if (needs_delete) delete this;
    }

private:
    Class *object_;
    MethodType method_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
    Arg4 arg4_;
    Arg5 arg5_;
};

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
class FunctionClosure6 : public Closure {
public:
    typedef void (*FunctionType)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6);

    FunctionClosure6(FunctionType function, bool self_deleting,
                     Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4,
                     Arg5 arg5, Arg6 arg6)
        : function_(function)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2)
        , arg3_(arg3)
        , arg4_(arg4)
        , arg5_(arg5)
        , arg6_(arg6) { }
    ~FunctionClosure6() { }

    void Run() {
        bool needs_delete = self_deleting_;
        function_(arg1_, arg2_, arg3_, arg4_, arg5_, arg6_);
        if (needs_delete) delete this;
    }

private:
    FunctionType function_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
    Arg4 arg4_;
    Arg5 arg5_;
    Arg6 arg6_;
};

template <typename Class ,typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
class MethodClosure6 : public Closure {
public:
    typedef void (Class::*MethodType)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6);

    MethodClosure6(Class *object, MethodType method, bool self_deleting,
                   Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4,
                   Arg5 arg5, Arg6 arg6)
        : object_(object)
        , method_(method)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2)
        , arg3_(arg3)
        , arg4_(arg4)
        , arg5_(arg5)
        , arg6_(arg6) { }
    ~MethodClosure6() { }

    void Run() {
        bool needs_delete = self_deleting_;
        (object_->*method_)(arg1_, arg2_, arg3_, arg4_, arg5_, arg6_);
        if (needs_delete) delete this;
    }

private:
    Class *object_;
    MethodType method_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
    Arg4 arg4_;
    Arg5 arg5_;
    Arg6 arg6_;
};

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7>
class FunctionClosure7 : public Closure {
public:
    typedef void (*FunctionType)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7);

    FunctionClosure7(FunctionType function, bool self_deleting,
                     Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4,
                     Arg5 arg5, Arg6 arg6, Arg7 arg7)
        : function_(function)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2)
        , arg3_(arg3)
        , arg4_(arg4)
        , arg5_(arg5)
        , arg6_(arg6)
        , arg7_(arg7) { }
    ~FunctionClosure7() { }

    void Run() {
        bool needs_delete = self_deleting_;
        function_(arg1_, arg2_, arg3_, arg4_, arg5_, arg6_, arg7_);
        if (needs_delete) delete this;
    }

private:
    FunctionType function_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
    Arg4 arg4_;
    Arg5 arg5_;
    Arg6 arg6_;
    Arg7 arg7_;
};

template <typename Class ,typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7>
class MethodClosure7 : public Closure {
public:
    typedef void (Class::*MethodType)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7);

    MethodClosure7(Class *object, MethodType method, bool self_deleting,
                   Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4,
                   Arg5 arg5, Arg6 arg6, Arg7 arg7)
        : object_(object)
        , method_(method)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2)
        , arg3_(arg3)
        , arg4_(arg4)
        , arg5_(arg5)
        , arg6_(arg6)
        , arg7_(arg7) { }
    ~MethodClosure7() { }

    void Run() {
        bool needs_delete = self_deleting_;
        (object_->*method_)(arg1_, arg2_, arg3_, arg4_, arg5_, arg6_, arg7_);
        if (needs_delete) delete this;
    }

private:
    Class *object_;
    MethodType method_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
    Arg4 arg4_;
    Arg5 arg5_;
    Arg6 arg6_;
    Arg7 arg7_;
};

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8>
class FunctionClosure8 : public Closure {
public:
    typedef void (*FunctionType)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8);

    FunctionClosure8(FunctionType function, bool self_deleting,
                     Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4,
                     Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8)
        : function_(function)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2)
        , arg3_(arg3)
        , arg4_(arg4)
        , arg5_(arg5)
        , arg6_(arg6)
        , arg7_(arg7)
        , arg8_(arg8) { }
    ~FunctionClosure8() { }

    void Run() {
        bool needs_delete = self_deleting_;
        function_(arg1_, arg2_, arg3_, arg4_, arg5_, arg6_, arg7_, arg8_);
        if (needs_delete) delete this;
    }

private:
    FunctionType function_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
    Arg4 arg4_;
    Arg5 arg5_;
    Arg6 arg6_;
    Arg7 arg7_;
    Arg8 arg8_;
};

template <typename Class ,typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8>
class MethodClosure8 : public Closure {
public:
    typedef void (Class::*MethodType)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8);

    MethodClosure8(Class *object, MethodType method, bool self_deleting,
                   Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4,
                   Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8)
        : object_(object)
        , method_(method)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2)
        , arg3_(arg3)
        , arg4_(arg4)
        , arg5_(arg5)
        , arg6_(arg6)
        , arg7_(arg7)
        , arg8_(arg8) { }
    ~MethodClosure8() { }

    void Run() {
        bool needs_delete = self_deleting_;
        (object_->*method_)(arg1_, arg2_, arg3_, arg4_, arg5_, arg6_, arg7_, arg8_);
        if (needs_delete) delete this;
    }

private:
    Class *object_;
    MethodType method_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
    Arg4 arg4_;
    Arg5 arg5_;
    Arg6 arg6_;
    Arg7 arg7_;
    Arg8 arg8_;
};

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
class FunctionClosure9 : public Closure {
public:
    typedef void (*FunctionType)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9);

    FunctionClosure9(FunctionType function, bool self_deleting,
                     Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4,
                     Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8,
                     Arg9 arg9)
        : function_(function)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2)
        , arg3_(arg3)
        , arg4_(arg4)
        , arg5_(arg5)
        , arg6_(arg6)
        , arg7_(arg7)
        , arg8_(arg8)
        , arg9_(arg9) { }
    ~FunctionClosure9() { }

    void Run() {
        bool needs_delete = self_deleting_;
        function_(arg1_, arg2_, arg3_, arg4_, arg5_, arg6_, arg7_, arg8_, arg9_);
        if (needs_delete) delete this;
    }

private:
    FunctionType function_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
    Arg4 arg4_;
    Arg5 arg5_;
    Arg6 arg6_;
    Arg7 arg7_;
    Arg8 arg8_;
    Arg9 arg9_;
};

template <typename Class ,typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
class MethodClosure9 : public Closure {
public:
    typedef void (Class::*MethodType)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9);

    MethodClosure9(Class *object, MethodType method, bool self_deleting,
                   Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4,
                   Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8,
                   Arg9 arg9)
        : object_(object)
        , method_(method)
        , self_deleting_(self_deleting)
        , arg1_(arg1)
        , arg2_(arg2)
        , arg3_(arg3)
        , arg4_(arg4)
        , arg5_(arg5)
        , arg6_(arg6)
        , arg7_(arg7)
        , arg8_(arg8)
        , arg9_(arg9) { }
    ~MethodClosure9() { }

    void Run() {
        bool needs_delete = self_deleting_;
        (object_->*method_)(arg1_, arg2_, arg3_, arg4_, arg5_, arg6_, arg7_, arg8_, arg9_);
        if (needs_delete) delete this;
    }

private:
    Class *object_;
    MethodType method_;
    bool self_deleting_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
    Arg4 arg4_;
    Arg5 arg5_;
    Arg6 arg6_;
    Arg7 arg7_;
    Arg8 arg8_;
    Arg9 arg9_;
};

} // namespace internal

inline Closure* NewCallback(void (*function)())
{
    return new internal::FunctionClosure0(function, true);
}

inline Closure* NewPermanentCallback(void (*function)())
{
    return new internal::FunctionClosure0(function, false);
}

template <typename Class>
inline Closure* NewCallback(Class* object, void (Class::*method)())
{
    return new internal::MethodClosure0<Class>(object, method, true);
}

template <typename Class>
inline Closure* NewPermanentCallback(Class* object, void (Class::*method)())
{
    return new internal::MethodClosure0<Class>(object, method, false);
}

template <typename Arg1>
inline Closure* NewCallback(void (*function)(Arg1), Arg1 arg1)
{
    return new internal::FunctionClosure1<Arg1>(function, true, arg1);
}

template <typename Arg1>
inline Closure* NewPermanentCallback(void (*function)(Arg1), Arg1 arg1)
{
    return new internal::FunctionClosure1<Arg1>(function, false, arg1);
}

template <typename Class, typename Arg1>
inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1), Arg1 arg1)
{
    return new internal::MethodClosure1<Class, Arg1>(object, method, true, arg1);
}

template <typename Class, typename Arg1>
inline Closure* NewPermanentCallback(Class* object, void (Class::*method)(Arg1), Arg1 arg1)
{
    return new internal::MethodClosure1<Class, Arg1>(object, method, false, arg1);
}

template <typename Arg1, typename Arg2>
inline Closure* NewCallback(void (*function)(Arg1, Arg2), Arg1 arg1, Arg2 arg2)
{
    return new internal::FunctionClosure2<Arg1, Arg2>(function, true, arg1, arg2);
}

template <typename Arg1, typename Arg2>
inline Closure* NewPermanentCallback(void (*function)(Arg1, Arg2), Arg1 arg1, Arg2 arg2)
{
    return new internal::FunctionClosure2<Arg1, Arg2>(function, false, arg1, arg2);
}

template <typename Class, typename Arg1, typename Arg2>
inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1, Arg2), Arg1 arg1, Arg2 arg2)
{
    return new internal::MethodClosure2<Class, Arg1, Arg2>(object, method, true, arg1, arg2);
}

template <typename Class, typename Arg1, typename Arg2>
inline Closure* NewPermanentCallback(Class* object, void (Class::*method)(Arg1, Arg2), Arg1 arg1, Arg2 arg2)
{
    return new internal::MethodClosure2<Class, Arg1, Arg2>(object, method, false, arg1, arg2);
}

template <typename Arg1, typename Arg2, typename Arg3>
inline Closure* NewCallback(void (*function)(Arg1, Arg2, Arg3), Arg1 arg1, Arg2 arg2, Arg3 arg3)
{
    return new internal::FunctionClosure3<Arg1, Arg2, Arg3>(function, true, arg1, arg2, arg3);
}

template <typename Arg1, typename Arg2, typename Arg3>
inline Closure* NewPermanentCallback(void (*function)(Arg1, Arg2, Arg3), Arg1 arg1, Arg2 arg2, Arg3 arg3)
{
    return new internal::FunctionClosure3<Arg1, Arg2, Arg3>(function, false, arg1, arg2, arg3);
}

template <typename Class, typename Arg1, typename Arg2, typename Arg3>
inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1, Arg2, Arg3), Arg1 arg1, Arg2 arg2, Arg3 arg3)
{
    return new internal::MethodClosure3<Class, Arg1, Arg2, Arg3>(object, method, true, arg1, arg2, arg3);
}

template <typename Class, typename Arg1, typename Arg2, typename Arg3>
inline Closure* NewPermanentCallback(Class* object, void (Class::*method)(Arg1, Arg2, Arg3), Arg1 arg1, Arg2 arg2, Arg3 arg3)
{
    return new internal::MethodClosure3<Class, Arg1, Arg2, Arg3>(object, method, false, arg1, arg2, arg3);
}

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
inline Closure* NewCallback(void (*function)(Arg1, Arg2, Arg3, Arg4), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
{
    return new internal::FunctionClosure4<Arg1, Arg2, Arg3, Arg4>(function, true, arg1, arg2, arg3, arg4);
}

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
inline Closure* NewPermanentCallback(void (*function)(Arg1, Arg2, Arg3, Arg4), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
{
    return new internal::FunctionClosure4<Arg1, Arg2, Arg3, Arg4>(function, false, arg1, arg2, arg3, arg4);
}

template <typename Class, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1, Arg2, Arg3, Arg4), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
{
    return new internal::MethodClosure4<Class, Arg1, Arg2, Arg3, Arg4>(object, method, true, arg1, arg2, arg3, arg4);
}

template <typename Class, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
inline Closure* NewPermanentCallback(Class* object, void (Class::*method)(Arg1, Arg2, Arg3, Arg4), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
{
    return new internal::MethodClosure4<Class, Arg1, Arg2, Arg3, Arg4>(object, method, false, arg1, arg2, arg3, arg4);
}

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
inline Closure* NewCallback(void (*function)(Arg1, Arg2, Arg3, Arg4, Arg5), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
{
    return new internal::FunctionClosure5<Arg1, Arg2, Arg3, Arg4, Arg5>(function, true, arg1, arg2, arg3, arg4, arg5);
}

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
inline Closure* NewPermanentCallback(void (*function)(Arg1, Arg2, Arg3, Arg4, Arg5), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
{
    return new internal::FunctionClosure5<Arg1, Arg2, Arg3, Arg4, Arg5>(function, false, arg1, arg2, arg3, arg4, arg5);
}

template <typename Class, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1, Arg2, Arg3, Arg4, Arg5), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
{
    return new internal::MethodClosure5<Class, Arg1, Arg2, Arg3, Arg4, Arg5>(object, method, true, arg1, arg2, arg3, arg4, arg5);
}

template <typename Class, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
inline Closure* NewPermanentCallback(Class* object, void (Class::*method)(Arg1, Arg2, Arg3, Arg4, Arg5), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
{
    return new internal::MethodClosure5<Class, Arg1, Arg2, Arg3, Arg4, Arg5>(object, method, false, arg1, arg2, arg3, arg4, arg5);
}

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
inline Closure* NewCallback(void (*function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6)
{
    return new internal::FunctionClosure6<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>(function, true, arg1, arg2, arg3, arg4, arg5, arg6);
}

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
inline Closure* NewPermanentCallback(void (*function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6)
{
    return new internal::FunctionClosure6<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>(function, false, arg1, arg2, arg3, arg4, arg5, arg6);
}

template <typename Class, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6)
{
    return new internal::MethodClosure6<Class, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>(object, method, true, arg1, arg2, arg3, arg4, arg5, arg6);
}

template <typename Class, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
inline Closure* NewPermanentCallback(Class* object, void (Class::*method)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6)
{
    return new internal::MethodClosure6<Class, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6>(object, method, false, arg1, arg2, arg3, arg4, arg5, arg6);
}

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7>
inline Closure* NewCallback(void (*function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7)
{
    return new internal::FunctionClosure7<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>(function, true, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7>
inline Closure* NewPermanentCallback(void (*function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7)
{
    return new internal::FunctionClosure7<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>(function, false, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

template <typename Class, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7>
inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7)
{
    return new internal::MethodClosure7<Class, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>(object, method, true, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

template <typename Class, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7>
inline Closure* NewPermanentCallback(Class* object, void (Class::*method)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7)
{
    return new internal::MethodClosure7<Class, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>(object, method, false, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8>
inline Closure* NewCallback(void (*function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8)
{
    return new internal::FunctionClosure8<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>(function, true, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
}

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8>
inline Closure* NewPermanentCallback(void (*function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8)
{
    return new internal::FunctionClosure8<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>(function, false, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
}

template <typename Class, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8>
inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8)
{
    return new internal::MethodClosure8<Class, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>(object, method, true, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
}

template <typename Class, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8>
inline Closure* NewPermanentCallback(Class* object, void (Class::*method)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8)
{
    return new internal::MethodClosure8<Class, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>(object, method, false, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
}

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
inline Closure* NewCallback(void (*function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9)
{
    return new internal::FunctionClosure9<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9>(function, true, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
}

template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
inline Closure* NewPermanentCallback(void (*function)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9)
{
    return new internal::FunctionClosure9<Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9>(function, false, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
}

template <typename Class, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9)
{
    return new internal::MethodClosure9<Class, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9>(object, method, true, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
}

template <typename Class, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
inline Closure* NewPermanentCallback(Class* object, void (Class::*method)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9), Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9)
{
    return new internal::MethodClosure9<Class, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9>(object, method, false, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
}

} // namespace qrpc

#endif /* QRPC_RPC_CLOSURE_H */
