# Advanced future
An std::future powerfull addition that enables `then` and `contraction` operations.

# Usage examples
Suppose we want to execute some task right after finish of some other task.
We can do it very simply:
```CPP
my_async([] { std::cout << "First message" << std::endl; })
    .then([] { std::cout << "Second message" << std::endl; }).get();
```

But what if we want to forward result of one task as an argument to another?
As easy as pie:
```CPP
auto result = my_async([](auto a, auto b) { return very_heavy_calculation(a, b) }, num1, num2)
                  .then([](auto a, auto b) { return another_calculation(a, b); }, num3);
return result.get();
```
You can see, that for the call to lambda inside `.then()` we specialized only one argument - the second one.
The first one would be forwarded from return value of lambda inside `my_async()` call.

And what if we want to make some dependent task that is represented as a sync for (as an example) 3 other tasks?
We can use `contraction()` function to do so:
```CPP
return contraction([](auto a, auto b, auto c) { return dependent_calculation(a, b, c); },
           [] { return first_very_heavy_calculation(); },
           [] { return second_maybe_not_so_heavy_calculation(); },
           [] { return third_independent_calculation(); }).get();
```

New features are in progress. Enjoy!
