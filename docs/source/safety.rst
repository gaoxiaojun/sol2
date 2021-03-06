safety
======

Sol was designed to be correct and fast, and in the pursuit of both uses the regular ``lua_to{x}`` functions of Lua rather than the checking versions (``lua_check{X}``) functions. The API defaults to paranoidly-safe alternatives if you have a ``#define SOL_CHECK_ARGUMENTS`` before you include Sol, or if you pass the ``SOL_CHECK_ARGUMENTS`` define on the build command for your build system. By default, it is off and remains off unless you define this, even in debug mode. The same goes for ``#define SOL_SAFE_USERTYPE``.

config
------

Note that you can obtain safety with regards to functions you bind by using the :doc:`protect<api/protect>` wrapper around function/variable bindings you set into Lua. Additionally, you can have basic boolean checks when using the API by just converting to a :doc:`sol::optional\<T><api/optional>` when necessary for getting things out of Lua and for function arguments.

``SOL_SAFE_USERTYPE`` triggers the following change:
	* If the userdata to a usertype function is nil, will trigger an error instead of letting things go through and letting the system segfault.

``SOL_CHECK_ARGUMENTS`` triggers the following changes:
	* ``sol::stack::get`` (used everywhere) defaults to using ``sol::stack::check_get`` and dereferencing the argument. It uses ``sol::type_panic`` as the handler if something goes wrong.
	* ``sol::stack::call`` and its variants will, if no templated boolean is specified, check all of the arguments for a function call.
	* If ``SOL_SAFE_USERTYPE`` is not defined, it gets defined to turn being on.

Tests are compiled with this on to ensure everything is going as expected. Remember that if you want these features, you must explicitly turn them on. 

Finally, some warnings that may help with errors when working with Sol:


functions
---------

The *vast majority* of all users are going to want to work with :doc:`sol::safe_function/sol::protected_function<api/protected_function>`. This version allows for error checking, prunes results, and responds to the defines listed above by throwing errors if you try to use the result of a function without checking. :doc:`sol::function/sol::unsafe_function<api/function>` is unsafe. It assumes that its contents run correctly and throw no errors, which can result in crashes that are hard to debug while offering a very tiny performance boost for not checking error codes or catching exceptions.

If you find yourself crashing inside of ``sol::function``, try changing it to a ``sol::protected_function`` and seeing if the error codes and such help you find out what's going on. You can read more about the API on :doc:`the page itself<api/protected_function>`.

As a side note, binding functions with default parameters does not magically bind multiple versions of the function to be called with the default parameters. You must instead use :doc:`sol::overload<api/overload>`.

.. warning::

	Do NOT save the return type of a :ref:`function_result<function-result>` with ``auto``, as in ``auto numwoof = woof(20);``, and do NOT store it anywhere. See :ref:`here<function-result-warning>`.