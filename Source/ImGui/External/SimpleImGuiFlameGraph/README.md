# SimpleImGuiFlameGraph

There are already flame graph implementations. But I think that they lose the extensibility and simplicity.

[https://github.com/RudjiGames/rprof](https://github.com/RudjiGames/rprof) looks nice,. But it is complex.

[https://github.com/bwrsandman/imgui-flame-graph](https://github.com/bwrsandman/imgui-flame-graph) is simple. But it only measure fix type of events.

So I code a widget that use scope timer to measure time, and use simple draw function to draw flame graph, which is less than 100 lines.

## Build

```shell
cmake -S . -B build
cmake --build build/
```

## Example

Running the `SimpleImGuiFlameGraphExample` example will display an example window showcasing the flame graph widget. The code is available for reference in `example/main.cpp`.

```cpp
TestFun();
FlameGraphDrawer::Draw(TimerSingleton::Get().GetScopeTimes(),
                        TimerSingleton::Get().GetMaxDepth(),
                        TimerSingleton::Get().GetGlobalStart());
TimerSingleton::Get().Clear();
```

![](./flame_graph.gif)

## Advantages

1.Time measurement and flame graph drawing are separated.

Example:

```cpp
TestFun();
FlameGraphDrawer::Draw(TimerSingleton::Get().GetScopeTimes(),
                        TimerSingleton::Get().GetMaxDepth(),
                        TimerSingleton::Get().GetGlobalStart());
TimerSingleton::Get().Clear();
```

Implementation code of `FlameGraphDrawer::Draw` is simple. Less than 100 lines.

2.Time measurement can be used in any scope, only one line of macro code is needed.

Example:

```cpp
void TestFun()
{
    FUNCTION_TIMER();

    std::random_device              rd;        // Will be used to obtain a seed for the random number engine
    std::mt19937                    gen(rd()); // Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> dis(1, 3000);

    {
        SCOPE_TIMER("Test Scope 1");

        int count = dis(gen);
        for (int i = 0; i < count; i++)
        {
            int x = dis(gen);
        }

        {
            SCOPE_TIMER("Test Scope 2");

            int count2 = dis(gen);
            for (int i = 0; i < count2; i++)
            {
                int x = dis(gen);
            }
        }

        {
            SCOPE_TIMER("Test Scope 3");

            int count3 = dis(gen);
            for (int i = 0; i < count3; i++)
            {
                int x = dis(gen);
            }
        }
    }

    {
        SCOPE_TIMER("Test Scope 4");

        int count = dis(gen);
        for (int i = 0; i < count; i++)
        {
            int x = dis(gen);
        }
    }
}
```

## Reference

`Timer` class refers to [https://github.com/TheCherno/Hazel](https://github.com/TheCherno/Hazel)

`FlameGraphDrawer` class refers to [https://github.com/bwrsandman/imgui-flame-graph](https://github.com/bwrsandman/imgui-flame-graph)