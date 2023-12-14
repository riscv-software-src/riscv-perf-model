
## Developing on Olympia

Developing on Olympia is welcomed and encouraged.  But there are a few
coding style and development guidelines that need to be adhered to.

If code is found to violate these rules -- please fix it!


### General rules

   * Branch naming must include your username and a very brief description
     of the issue at hand. For example, fix for issue 1234 which might
     be a seg fault in LSU: git branch knutel/issue_1234_lsu_seg_fault
   * Any bug fix/enhancement _must be accompanied_ with a test that
     illustrates the bug fix/enhancement.  No test == no acceptance.
   * Testing can be either be made within an existing test or the
     addition of a new test.  The _test must be exercised_ by the `regress`
     target.
   * Documentation fixes do not require a test (but grammar/spelling
     issues should be fixed :wink: )
   * As a general rule on development, adhere to the **existing coding
     style** of the file being modified.  Look at the existing code and
     _mimic it_.  Don't mix another preferred style with the existing
     style -- stick with the original style.

### New and Existing Development

   1. Class names are `CamelCase` with the Camel's head up: `class
      OlympiaUnit`
   1. Public class method names are `camelCase` with the camel's head
      down: `void myMethod()`
   1. Private/protected class method names are `camelCase_` with the
      camel's head down and a trailing `_`: `void myMethod_()`
   1. Member variable names that are `private` or `protected` must be all
      `lower_case_` with a trailing `_` so developers know a member
      variable is private or protected.
   1. Place an `_` between words on member variables: `my_data_member_`
   1. Header file guards are `#pragma once`
   1. Any function/class from `std` namespace must always be explicit:
      `std::vector` NOT `vector`.  Never use `using namespace <blah>;` in
      *any* header file
   1. Consider using source files for non-critical path code
   1. Try to keep methods short and concise
   1. Do not go nuts with `auto`.  This: `auto foo(const auto & in)` is irritating
   1. NO TABS.  Ever.  Never ever.
   1. Spaces are 4
   1. Delete all trailing whitespace
   1. Do not use single letter variable names.  Not only are these NOT
      descriptive, searching for their use in a function/class is difficult.
   1. Do not use Raw POD types (int, char).  Instead use [fixed
      width integer types](https://en.cppreference.com/w/cpp/types/integer)
      like uint32_t.
   1. Avoid using signed integers
   1. Curly braces _must be included_ around each `if`, `for`, `while`, etc
      statement.  This specifically pertains to single line statements:
      `if(condition) { statement; }`
    1. Limit column width to 100 characters

### Coding style for using Sparta components

   1. When declaring a Port, name the variable with either an `in_`
      prefix or an `out_` prefix
   1. When declaring an Event, name the variable with an `ev_` prefix
   1. When declaring a Statistic or Counter, name the stat/counter
      variable with the _same name_ as the stat/counter that shows up in
      a report.
   1. To improve simulation performance, copy a parameter value to a local
      `const` data member in the initialization list:
      `my_parameter_value_(params->my_parameter_value)`

Example class style, header file:

```
// Only include this file once
#pragma once

#include "sparta/simulation/Unit.hpp"

// other sparta headers...

namespace sparta {
    class TreeNode;
}

namespace olympia
{
    class MyUnitParameters;

    //
    // Comment on what this unit does
    //
    class MyUnit : public sparta::Unit
    {
    public:
        // MyUnit's constructor
        MyUnit(sparta::TreeNode * node,
               const MyUnitParameters * params);

        // A public method
        void myUnitPublicMethod();

    protected:

        // A protected method
        void myUnitProtectedMethod_();

        // A protected variable, initialized here
        uint32_t my_protected_variable_ = 0;

    private:

        // A private method
        void myUnitPrivateMethod_();

        // A private variable, initialized here
        uint32_t my_private_variable_ = 0;

        //////////////////////////////////////////////////////////////////////
        // Ports
        //////////////////////////////////////////////////////////////////////

        sparta::InPort  in_my_port_;
        sparta::OutPort out_my_port_;

        //////////////////////////////////////////////////////////////////////
        // Events
        //////////////////////////////////////////////////////////////////////

        sparta::Event ev_my_event_;

        //////////////////////////////////////////////////////////////////////
        // Stats
        //////////////////////////////////////////////////////////////////////

        sparta::Counter my_stat_; // Named my_stat in the report

    };
```
