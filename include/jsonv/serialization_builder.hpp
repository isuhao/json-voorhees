/** \file jsonv/serialization_builder.hpp
 *  DSL for building \c formats.
 *  
 *  Copyright (c) 2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_SERIALIZATION_BUILDER_HPP_INCLUDED__
#define __JSONV_SERIALIZATION_BUILDER_HPP_INCLUDED__

#include <jsonv/config.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/serialization_util.hpp>

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <type_traits>

namespace jsonv
{

/** \page serialization_builder_dsl Serialization Builder DSL
 *  
 *  Most applications tend to have a lot of structure types. While it is possible to write an \c extractor and
 *  \c serializer (or \c adapter) for each type, this can get a little bit tedious. Beyond that, it is very difficult to
 *  look at the contents of adapter code and discover what the JSON might actually look like. The builder DSL is meant
 *  to solve these issues by providing a convenient way to describe conversion operations for your C++ types.
 *  
 *  At the end of the day, the goal is to take some C++ structures like this:
 *  
 *  \code
 *  struct person
 *  {
 *      std::string first_name;
 *      std::string last_name;
 *      int         age;
 *      std::string role;
 *  };
 *  
 *  struct company
 *  {
 *      std::string         name;
 *      bool                certified;
 *      std::vector<person> employees;
 *      std::list<person>   candidates;
 *  };
 *  \endcode
 *  
 *  ...and easily convert it to an from a JSON representation that looks like this:
 *  
 *  \code
 *  {
 *      "name": "Paul's Construction",
 *      "certified": false,
 *      "employees": [
 *          {
 *              "first_name": "Bob",
 *              "last_name":  "Builder",
 *              "age":        29
 *          },
 *          {
 *              "first_name": "James",
 *              "last_name":  "Johnson",
 *              "age":        38,
 *              "role":       "Foreman"
 *          }
 *      ],
 *      "candidates": [
 *          {
 *              "firstname": "Adam",
 *              "lastname":  "Ant"
 *          }
 *      ]
 *  }
 *  \endcode
 *  
 *  To define a \c formats for this \c person type using the serialization builder DSL, you would say:
 *  
 *  \code
 *  jsonv::formats fmts =
 *      jsonv::formats_builder()
 *          .type<person>()
 *              .member("first_name", &person::first_name)
 *                  .alternate_name("firstname")
 *              .member("last_name",  &person::last_name)
 *                  .alternate_name("lastname")
 *              .member("age",        &person::age)
 *                  .until({ 6,1 })
 *                  .default_value(21)
 *                  .default_on_null()
 *                  .check_input([] (int value) { if (value < 0) throw std::logic_error("Age must be positive."); })
 *              .member("role",       &person::role)
 *                  .since({ 2,0 })
 *                  .default_value("Builder")
 *          .type<company>()
 *              .member("name",       &company::name)
 *              .member("certified",  &company::certified)
 *              .member("employees",  &company::employees)
 *              .member("candidates", &company::candidates)
 *          .register_containers<company, std::vector, std::list>()
 *          .check_references()
 *      ;
 *  \endcode
 * 
 *  \section Reference
 *  
 *  The DSL is made up of three major parts:
 *  
 *   1. \e formats -- modifies a \c jsonv::formats object by adding new type adapters to it
 *   2. \e type -- modifies the behavior of a \c jsonv::adapter by adding new members to it
 *   3. \e member -- modifies an individual member inside of a specific type
 *  
 *  Each successive function call transforms your context. \e Narrowing calls make your context more specific; for
 *  example, calling \c type from a \e formats context allows you to modify a specific type. \e Widening calls make the
 *  context less specific and are always available; for example, when in the \e member context, you can still call
 *  \c type from the \e formats context to specify a new type.
 *  
 *  \dot
 *  digraph serialization_builder_dsl {
 *    formats  [label="formats"]
 *    type     [label="type"]
 *    member   [label="member"]
 *    
 *    formats -> formats
 *    formats -> type
 *    type    -> formats
 *    type    -> type
 *    type    -> member
 *    member  -> formats
 *    member  -> type
 *    member  -> member
 *  }
 *  \enddot
 *  
 *  \subsection serialization_builder_dsl_ref_formats Formats Context
 *  
 *  Commands in this section modify the behavior of the underlying \c jsonv::formats object.
 *  
 *  \subsubsection serialization_builder_dsl_ref_formats_level Level
 *  
 *  \paragraph serialization_builder_dsl_ref_formats_level_check_references check_references
 *  
 *   - <tt>check_references(formats)</tt>
 *   - <tt>check_references(formats, std::string name)</tt>
 *  
 *  Tests that every type referenced by the members of the output of the DSL have an \c extractor and a \c serializer.
 *  The provided \c formats is used to draw extra types from (a common value is \c jsonv::formats::defaults). In other
 *  words, it asks the question: If the \c formats from this DSL was combined with these other \c formats, could all of
 *  the types be encoded and decoded?
 *  
 *  This does not mutate the DSL in any way. On successful verification, it will appear that nothing happened. If the
 *  verification is not successful, an exception will be thrown with the offending types in the message. For example:
 *  
 *  \code
 *  There are 2 types referenced that the formats do not know how to serialize: 
 *   - date_type (referenced by: name_space::foo, other::name::space::bar)
 *   - tree
 *  \endcode
 *  
 *  If \a name is provided, the value will be output to the error message on failure. This can be useful if you have
 *  multiple \c check_references statements and wish to more easily determine the failing \c formats combination from
 *  the error message alone.
 *  
 *  \note
 *  This is evaluated \e immediately, so it is best to call this function as the very last step in the DSL.
 *  
 *  \code
 *    .check_references()
 *  \endcode
 *  
 *  \paragraph serialization_builder_dsl_ref_formats_level_reference_type reference_type
 *  
 *   - <tt>reference_type(std::type_index type)</tt>
 *   - <tt>reference_type(std::type_index type, std::type_index from)</tt>
 *  
 *  Explicitly add a reference to the provided \a type in the DSL. If \a from is provided, also add a back reference for
 *  tracking purposes. The \a from field is useful for tracking \e why the \a type is referenced.
 *  
 *  Type references are used in \ref serialization_builder_dsl_ref_formats_level_check_references to both check and
 *  generate error messages if the \c formats the DSL is building cannot fully create and extract JSON values. You do
 *  not usually have to call this, as each call to \ref serialization_builder_dsl_ref_type_narrowing_member calls this
 *  automatically.
 *  
 *  \code
 *    .reference_type(std::type_index(typeid(int)), std::type_index(typeid(my_type)))
 *    .reference_type(std::type_index(typeid(my_type))
 *  \endcode
 *  
 *  \paragraph serialization_builder_dsl_ref_formats_level_register_adapter register_adapter
 *  
 *   - <tt>register_adapter(const adapter*)</tt>
 *   - <tt>register_adapter(std::shared_ptr&lt;const adapter&gt;)</tt>
 *  
 *  Register an arbitrary \c adapter with the \c formats we are currently building. This is useful for integrating with
 *  type adapters that do not (or can not) use the DSL.
 *  
 *  \code
 *    .register_adapter(my_type::get_adapter())
 *  \endcode
 * 
 *  \paragraph serialization_builder_dsl_ref_formats_level_register_container register_container
 *  
 *   - <tt>register_container&lt;TContainer&gt;()</tt>
 *  
 *  Similar to \c register_adapter, but automatically create a <tt>container_adapter&lt;TContainer&gt;</tt> to store.
 *  
 *  \code
 *    .register_container<std::vector<int>>()
 *    .register_container<std::list<std::string>>()
 *  \endcode
 * 
 *  \paragraph serialization_builder_dsl_ref_formats_level_register_containers register_containers
 *  
 *   - <tt>register_containers&lt;T, template &lt;T, ...&gt;... TTContainer&gt;</tt>
 *  
 *  Convenience function for calling \c register_container for multiple containers with the same \c value_type.
 *  Unfortunately, it only supports varying the first template parameter of the \c TTContainer types, so if you wish to
 *  do something like vary the allocator, you will have to either call \c register_container multiple times or use a
 *  template alias.
 *  
 *  \code
 *    .register_containers<int, std::list, std::deque>()
 *    .register_containers<double, std::vector, std::set>()
 *  \endcode
 *  
 *  \note
 *  Not supported in MSVC 14 (CTP 5).
 *  
 *  \subsubsection serialization_builder_dsl_ref_formats_narrowing Narrowing
 *  
 *  \paragraph serialization_builder_dsl_ref_formats_narrowing_type type&lt;T&gt;
 *  
 *   - <tt>type&lt;T&gt;()</tt>
 *   - <tt>type&lt;T&gt;(std::function&lt;void (adapter_builder&lt;T&gt;)&gt; func)</tt>
 *  
 *  Create an \c adapter for type \c T and begin building the members for it. If \a func is provided, it will be called
 *  with the adapter_builder&lt;T&gt; this call to \c type creates, which can be used for creating common extension
 *  functions.
 *  
 *  \code
 *    .type<my_type>()
 *        .member(...)
 *        .
 *        .
 *        .
 *  \endcode
 *  
 *  
 *  \subsection serialization_builder_dsl_ref_type Type Context
 *  
 *  Commands in this section modify the behavior of the \c jsonv::adapter for a particular type.
 *  
 *  \subsubsection serialization_builder_dsl_ref_type_level Level
 *  
 *  \paragraph serialization_builder_dsl_ref_type_level_pre_extract pre_extract
 *  
 *   - <tt>pre_extract(std::function&lt;void (const extraction_context& context, const value& from)&gt; perform)</tt>
 *  
 *  Call the given \a perform function during the \c extract operation, but before performing any extraction. This can
 *  be called multiple times -- all functions will be called in the order they are provided.
 *  
 *  \paragraph serialization_builder_dsl_ref_type_level_on_extract_extra_keys on_extract_extra_keys
 *  
 *   - <tt>on_extract_extra_keys(std::function&lt;void (const extraction_context&   context,
 *                                                      const value&                from,
 *                                                      std::set&lt;std::string&gt; extra_keys)&gt; action
 *                              )</tt>
 *  
 *  When extracting, perform some \a action if extra keys are provided. By default, extra keys are usually simply
 *  ignored, so this is useful if you wish to throw an exception (or anything you want).
 *  
 *  \code
 *    .type<my_type>()
 *        .member("x", &my_type::x)
 *        .member("y", &my_type::y)
 *        .on_extract_extra_keys([] (const extraction_context&, const value&, std::set<std::string> extra_keys)
 *                               {
 *                                   throw extracted_extra_keys("my_type", std::move(extra_keys));
 *                               }
 *                              )
 *  \endcode
 *  
 *  \subsubsection serialization_builder_dsl_ref_type_narrowing Narrowing
 *  
 *  \paragraph serialization_builder_dsl_ref_type_narrowing_member member
 *  
 *   - <tt>member(std::string name, TMember T::*selector)</tt>
 *  
 *  Adds a member to the type we are currently building. By default, the member will be serialized with the key of the
 *  given \a name and the extractor will search for the given \a name. If you wish to change properties of this field,
 *  use the \ref serialization_builder_dsl_ref_member.
 *  
 *  \code
 *    .type<my_type>()
 *        .member("x", &my_type::x)
 *        .member("y", &my_type::y)
 *  \endcode
 *  
 *  
 *  \subsection serialization_builder_dsl_ref_member Member Context
 *  
 *  Commands in this section modify the behavior of a particular member. Here, \c T refers to the containing type (the
 *  one we are adding a member to) and \c TMember refers to the type of the member we are modifying.
 *  
 *  \subsubsection serialization_builder_dsl_ref_member_level Level
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_after after
 *  
 *   - <tt>after(version)</tt>
 *  
 *  Only serialize this member if the \c serialization_context::version is not \c version::empty and is greater than or
 *  equal to the provided \c version.
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_alternate_name alternate_name
 *  
 *   - <tt>alternate_name(std::string name)</tt>
 *  
 *  Provide an alternate name to search for when extracting this member. If a user provides values for multiple names,
 *  preference is given to names earlier in the list, starting with the original given name.
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_before before
 *  
 *   - <tt>before(version)</tt>
 *  
 *  Only serialize this member if the \c serialization_context::version is not \c version::empty and is less than or
 *  equal to the provided \c version.
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_check_input check_input
 *  
 *   - <tt>check_input(std::function&lt;void (const TMember&)&gt; check)</tt>
 *   - <tt>check_input(std::function&lt;bool (const TMember&)&gt; check, std::function&lt;void (const TMember&)&gt; thrower)</tt>
 *   - <tt>check_input(std::function&lt;bool (const TMember&)&gt; check, TException ex)</tt>
 *  
 *  Checks the extracted value with the given \a check function. In the first form, you are expected to throw inside the
 *  function. In the latter forms, the second parameter will be invoked (in the case of \a thrower) or thrown directly
 *  (in the case of \a ex).
 *  
 *  \code
 *    .member("x", &my_type::x)
 *        .check([] (int x) { if (x < 0) throw std::logic_error("x must be greater than 0"); })
 *        .check([] (int x) { return x < 100; }, [] (int x) { throw exceptions::less_than(100, x); })
 *        .check([] (int x) { return x % 2 == 0; }, std::logic_error("x must be divisible by 2"))
 *  \endcode
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_default_value default_value
 *  
 *   - <tt>default_value(TMember value)</tt>
 *   - <tt>default_value(std::function&lt;TMember (const extraction_context&, const value&)&gt; create)</tt>
 *  
 *  Provide a default value for this member if no key is found when extracting. You can use the function implementation
 *  to synthesize the key however you want.
 *  
 *  \code
 *   .member("x", &my_type::x)
 *       .default_value(10)
 *  \endcode
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_default_on_null default_on_null
 *  
 *   - <tt>default_on_null()</tt>
 *   - <tt>default_on_null(bool on)</tt>
 *  
 *  If the value associated with this key is \c kind::null, should that be treated as the default value? This option is
 *  only considered if a \ref serialization_builder_dsl_ref_member_level_default_value default_value was provided.
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_encode_if encode_if
 *  
 *   - <tt>encode_if(std::function&lt;bool (const serialization_context&, const TMember&amp;)&gt; check)</tt>
 *  
 *  Only serialize this member if the \a check function returns true.
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_since since
 *  
 *   - <tt>since(version)</tt>
 *  
 *  Only serialize this member if the \c serialization_context::version is not \c version::empty and is greater than the
 *  provided \c version.
 *  
 *  \paragraph serialization_builder_dsl_ref_member_level_until until
 *  
 *   - <tt>until(version)</tt>
 *  
 *  Only serialize this member if the \c serialization_context::version is not \c version::empty and is less than the
 *  provided \c version.
**/

class formats_builder;

namespace detail
{

template <typename T> class adapter_builder;
template <typename T, typename TMember> class member_adapter_builder;

class formats_builder_dsl
{
public:
    explicit formats_builder_dsl(formats_builder* owner) :
            owner(owner)
    { }
    
    template <typename T>
    detail::adapter_builder<T> type();
    
    template <typename T, typename F>
    detail::adapter_builder<T> type(F&&);
    
    formats_builder& register_adapter(const adapter* p);
    formats_builder& register_adapter(std::shared_ptr<const adapter> p);
    
    formats_builder& reference_type(std::type_index typ);
    formats_builder& reference_type(std::type_index type, std::type_index from);
    
    formats_builder& check_references(formats other, const std::string& name = "");
    
    template <typename TContainer>
    formats_builder& register_container();
    
    #if JSONV_COMPILER_SUPPORTS_TEMPLATE_TEMPLATES
    template <typename T, template <class...> class... TTContainers>
    formats_builder& register_containers();
    #endif
    
    operator formats() const;
    
protected:
    formats_builder* owner;
};

template <typename T>
class adapter_builder_dsl
{
public:
    explicit adapter_builder_dsl(adapter_builder<T>* owner) :
            owner(owner)
    { }
    
    template <typename TMember>
    member_adapter_builder<T, TMember> member(std::string name, TMember T::*selector);
    
    adapter_builder<T>& pre_extract(std::function<void (const extraction_context&, const value& from)> perform);
    
    adapter_builder<T>& on_extract_extra_keys(std::function<void (const extraction_context&, const value& from, std::set<std::string> extra_keys)> handler);
    
protected:
    adapter_builder<T>* owner;
};

template <typename T>
class member_adapter
{
public:
    virtual ~member_adapter() noexcept
    { }
    
    virtual void mutate(const extraction_context& context, const value& from, T& out) const = 0;
    
    virtual void to_json(const serialization_context& context, const T& from, value& out) const = 0;
    
    virtual bool has_extract_key(string_view key) const = 0;
};

template <typename T, typename TMember>
class member_adapter_impl :
        public member_adapter<T>
{
public:
    explicit member_adapter_impl(std::string name, TMember T::*selector) :
            _names({ std::move(name) }),
            _selector(selector)
    { }
    
    virtual void mutate(const extraction_context& context, const value& from, T& out) const override
    {
        value::const_object_iterator iter;
        for (const auto& name : _names)
            if ((iter = from.find(name)) != from.end_object())
                break;
        
        bool use_default = false;
        if (_default_value)
        {
            use_default = (iter == from.end_object())
                       || (_default_on_null && iter->second.kind() == kind::null);
        }
        
        if (use_default)
            (out.*_selector) = _default_value(context, from);
        else
            (out.*_selector) = context.extract_sub<TMember>(from, iter->first);
    }
    
    virtual void to_json(const serialization_context& context, const T& from, value& out) const override
    {
        if (should_encode(context, from))
            out.insert({ _names.at(0), context.to_json(from.*_selector) });
    }
    
    virtual bool has_extract_key(string_view key) const override
    {
        return std::any_of(begin(_names), end(_names), [key] (const std::string& name) { return name == key; });
    }
    
    void add_encode_check(std::function<bool (const serialization_context&, const TMember&)> check)
    {
        if (_should_encode)
        {
            auto old_check = std::move(_should_encode);
            _should_encode = [check, old_check] (const serialization_context& context, const TMember& value)
                             {
                                return check(context, value) && old_check(context, value);
                             };
        }
        else
        {
            _should_encode = std::move(check);
        }
    }
    
    void add_extraction_mutator(std::function <TMember (TMember&&)> mutate)
    {
        if (_extract_mutate)
        {
            auto old_mutate = std::move(_extract_mutate);
            _extract_mutate = [old_mutate, mutate] (TMember&& member) { return mutate(old_mutate(std::move(member))); };
        }
        else
        {
            _extract_mutate = std::move(mutate);
        }
    }
    
    void add_extraction_check(std::function <void (const TMember&)> check)
    {
        add_extraction_mutator([check] (TMember&& value)
        {
            check(value);
            return value;
        });
    }
    
private:
    bool should_encode(const serialization_context& context, const T& from) const
    {
        if (_should_encode)
            return _should_encode(context, from.*_selector);
        else
            return true;
    }
    
private:
    template <typename U, typename UMember>
    friend class member_adapter_builder;
    
private:
    std::vector<std::string>                                           _names;
    TMember T::*                                                       _selector;
    std::function<bool (const serialization_context&, const TMember&)> _should_encode;
    std::function<TMember (const extraction_context&, const value&)>   _default_value;
    bool                                                               _default_on_null = true;
    std::function<TMember (TMember&&)>                                 _extract_mutate;
};

template <typename T, typename TMember>
class member_adapter_builder :
        public formats_builder_dsl,
        public adapter_builder_dsl<T>
{
public:
    explicit member_adapter_builder(formats_builder*                 fmt_builder,
                                    adapter_builder<T>*              adapt_builder,
                                    member_adapter_impl<T, TMember>* adapter
                                   ) :
            formats_builder_dsl(fmt_builder),
            adapter_builder_dsl<T>(adapt_builder),
            _adapter(adapter)
    {
        reference_type(std::type_index(typeid(TMember)), std::type_index(typeid(T)));
    }
    
    /** When extracting, also look for this \a name as a key. **/
    member_adapter_builder& alternate_name(std::string name)
    {
        _adapter->_names.emplace_back(std::move(name));
        return *this;
    }
    
    member_adapter_builder& check_input(std::function<void (const TMember&)> check)
    {
        _adapter->add_extraction_check(std::move(check));
        return *this;
    }
    
    member_adapter_builder& check_input(std::function<bool (const TMember&)> check,
                                        std::function<void (const TMember&)> thrower
                                       )
    {
        _adapter->add_extraction_check([check, thrower] (const TMember& value)
            {
                if (!check(value))
                    thrower(value);
            });
        return *this;
    }
    
    template <typename TException>
    member_adapter_builder& check_input(std::function<void (const TMember&)> check, const TException& ex)
    {
        return check_input(std::move(check), [ex] (const TMember&) { throw ex; });
    }
    
    /** If the key for this member is not in the object when deserializing, call this function to create a value. If a
     *  \c default_value is not specified, the key is required.
    **/
    member_adapter_builder& default_value(std::function<TMember (const extraction_context&, const value&)> create)
    {
        _adapter->_default_value = std::move(create);
        return *this;
    }
    
    /** If the key for this member is not in the object when deserializing, use this \a value. If a \c default_value is
     *  not specified, the key is required.
    **/
    member_adapter_builder& default_value(TMember value)
    {
        return default_value([value] (const extraction_context&, const jsonv::value&) { return value; });
    }
    
    /** Should a \c kind::null for a key be interpreted as a missing value? **/
    member_adapter_builder& default_on_null(bool on = true)
    {
        _adapter->_default_on_null = on;
        return *this;
    }
    
    /** Only encode this member if the \a check passes. The final decision to encode is based on \e all \c check
     *  functions.
    **/
    member_adapter_builder& encode_if(std::function<bool (const serialization_context&, const TMember&)> check)
    {
        _adapter->add_encode_check(std::move(check));
        return *this;
    }
    
    /** Only encode this member if the \c serialization_context::version is greater than or equal to \a ver. **/
    member_adapter_builder& since(version ver)
    {
        return encode_if([ver] (const serialization_context& context, const TMember&)
                         {
                             return context.version().empty() || context.version() >= ver;
                         }
                        );
    }
    
    /** Only encode this member if the \c serialization_context::version is less than or equal to \a ver. **/
    member_adapter_builder& until(version ver)
    {
        return encode_if([ver] (const serialization_context& context, const TMember&)
                         {
                             return context.version().empty() || context.version() <= ver;
                         }
                        );
    }
    
    /** Only encode this member if the \c serialization_context::version is greater than \a ver. **/
    member_adapter_builder& after(version ver)
    {
        return encode_if([ver] (const serialization_context& context, const TMember&)
                         {
                             return context.version().empty() || context.version() > ver;
                         }
                        );
    }
    
    /** Only encode this member if the \c serialization_context::version is less than \a ver. **/
    member_adapter_builder& before(version ver)
    {
        return encode_if([ver] (const serialization_context& context, const TMember&)
                         {
                             return context.version().empty() || context.version() < ver;
                         }
                        );
    }
    
private:
    member_adapter_impl<T, TMember>* _adapter;
};

template <typename T>
class adapter_builder :
        public formats_builder_dsl
{
public:
    template <typename F>
    explicit adapter_builder(formats_builder* owner, F&& f) :
            formats_builder_dsl(owner),
            _adapter(nullptr)
    {
        auto adapter = std::make_shared<adapter_impl>();
        register_adapter(adapter);
        _adapter = adapter.get();
        
        std::forward<F>(f)(*this);
    }
    
    explicit adapter_builder(formats_builder* owner) :
            adapter_builder(owner, [] (const adapter_builder<T>&) { })
    { }
    
    template <typename TMember>
    member_adapter_builder<T, TMember> member(std::string name, TMember T::*selector)
    {
        std::unique_ptr<member_adapter_impl<T, TMember>> ptr
            (
                new member_adapter_impl<T, TMember>(std::move(name), selector)
            );
        member_adapter_builder<T, TMember> builder(formats_builder_dsl::owner, this, ptr.get());
        _adapter->_members.emplace_back(std::move(ptr));
        return builder;
    }
    
    adapter_builder<T>& pre_extract(std::function<void (const extraction_context&, const value& from)> perform)
    {
        if (_adapter->_pre_extract)
        {
            std::function<void (const extraction_context&, const value&)> old_perform = std::move(_adapter->_pre_extract);
            _adapter->_pre_extract = [old_perform, perform] (const extraction_context& context, const value& from)
                                     {
                                         old_perform(context, from);
                                         perform(context, from);
                                     };
        }
        else
        {
            _adapter->_pre_extract = std::move(perform);
        }
        return *this;
    }
    
    adapter_builder<T>& on_extract_extra_keys(std::function<void (const extraction_context&, const value& from, std::set<std::string> extra_keys)> handler)
    {
        adapter_impl* adapter = _adapter;
        return pre_extract([adapter, handler] (const extraction_context& context, const value& from)
        {
            auto is_key = [adapter] (string_view key) -> bool
                          {
                              return std::any_of(begin(adapter->_members), end(adapter->_members),
                                                 [key] (const std::unique_ptr<member_adapter<T>>& mem)
                                                 {
                                                     return mem->has_extract_key(key);
                                                 }
                                                );
                          };
            std::set<std::string> extra_keys;
            for (const auto& pair : from.as_object())
                if (!is_key(pair.first))
                    extra_keys.insert(pair.first);
            if (!extra_keys.empty())
                handler(context, from, std::move(extra_keys));
        });
    }
    
private:
    class adapter_impl :
            public adapter_for<T>
    {
    public:
    
        virtual T create(const extraction_context& context, const value& from) const override
        {
            if (_pre_extract)
                _pre_extract(context, from);
            
            T out;
            for (const auto& member : _members)
                member->mutate(context, from, out);
            return out;
        }
        
        virtual value to_json(const serialization_context& context, const T& from) const override
        {
            value out = object();
            for (const auto& member : _members)
                member->to_json(context, from, out);
            return out;
        }
        
        std::deque<std::unique_ptr<member_adapter<T>>>                     _members;
        std::function<void (const extraction_context&, const value& from)> _pre_extract;
    };
    
private:
    adapter_impl* _adapter;
};

}

class JSONV_PUBLIC formats_builder
{
public:
    formats_builder();
    
    template <typename T>
    detail::adapter_builder<T> type()
    {
        return detail::adapter_builder<T>(this);
    }
    
    template <typename T, typename F>
    detail::adapter_builder<T> type(F&& f)
    {
        return detail::adapter_builder<T>(this, std::forward<F>(f));
    }
    
    formats_builder& register_adapter(const adapter* p)
    {
        _formats.register_adapter(p);
        return *this;
    }
    
    formats_builder& register_adapter(std::shared_ptr<const adapter> p)
    {
        _formats.register_adapter(std::move(p));
        return *this;
    }
    
    template <typename TContainer>
    formats_builder& register_container()
    {
        std::unique_ptr<container_adapter<TContainer>> p(new container_adapter<TContainer>);
        _formats.register_adapter(std::move(p));
        return *this;
    }
    
    #if JSONV_COMPILER_SUPPORTS_TEMPLATE_TEMPLATES
    template <typename T>
    formats_builder& register_containers()
    {
        return *this;
    }
    
    template <typename T, template <class...> class TTContainer, template <class...> class... TTRest>
    formats_builder& register_containers()
    {
        register_container<TTContainer<T>>();
        return register_containers<T, TTRest...>();
    }
    #endif
    
    operator formats() const
    {
        return _formats;
    }
    
    formats_builder& reference_type(std::type_index type);
    formats_builder& reference_type(std::type_index type, std::type_index from);
    
    /** Check that, when combined with the \c formats \a other, all types referenced by this \c formats_builder will
     *  get decoded properly.
     *  
     *  \param name if non-empty and this function throws, this \a name will be provided in the exception's \c what
     *              string. This can be useful if you are running multiple \c check_references calls and you want to
     *              name the different checks.
     *  
     *  \throws std::logic_error if \c formats this \c formats_builder is generating, when combined with the provided
     *                           \a other \c formats, cannot properly serialize all the types.
    **/
    formats_builder& check_references(formats other, const std::string& name = "");
    
private:
    formats                                              _formats;
    std::map<std::type_index, std::set<std::type_index>> _referenced_types;
};

namespace detail
{

template <typename T>
adapter_builder<T> formats_builder_dsl::type()
{
    return owner->type<T>();
}

template <typename T, typename F>
adapter_builder<T> formats_builder_dsl::type(F&& f)
{
    return owner->type<T>(std::forward<F>(f));
}

template <typename TContainer>
formats_builder& formats_builder_dsl::register_container()
{
    return owner->register_container<TContainer>();
}

#if JSONV_COMPILER_SUPPORTS_TEMPLATE_TEMPLATES
template <typename T, template <class...> class... TTContainers>
formats_builder& formats_builder_dsl::register_containers()
{
    return owner->register_containers<T, TTContainers...>();
}
#endif

template <typename T>
template <typename TMember>
member_adapter_builder<T, TMember> adapter_builder_dsl<T>::member(std::string name, TMember T::*selector)
{
    return owner->member(std::move(name), selector);
}

template <typename T>
adapter_builder<T>& adapter_builder_dsl<T>
::pre_extract(std::function<void (const extraction_context&, const value& from)> perform)
{
    return owner->pre_extract(std::move(perform));
}

template <typename T>
adapter_builder<T>& adapter_builder_dsl<T>
::on_extract_extra_keys(std::function<void (const extraction_context&, const value& from, std::set<std::string> extra_keys)> handler)
{
    return owner->on_extract_extra_keys(std::move(handler));
}

}

}

#endif/*__JSONV_SERIALIZATION_BUILDER_HPP_INCLUDED__*/
