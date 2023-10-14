# <img src="Content/Editor/FunctionIcon.svg" height=40 width=40 align="center"/> Neat Functions
Unreal Engine plugin that extends UFunctions in some neat ways. Functions that take delegates can have their events created in the function node itself, making them look like async actions.

## Examples

### Simple
This is a simple example of what this plugin does, with the code used to create the nodes below. Note that by default the original function node will still be available. You have to explicitly remove the regular node if you want that, using the `BlueprintInternalUseOnly` metadata, as shown in `MyFunction2`.

![An example of this plugin, showing three nodes, two of which are created by this plugin.](Documentation/Example_01.png)
```c++
DECLARE_DYNAMIC_DELEGATE(FMyDelegate);

// This will show both the original function and the Neat version in the BP context menu
UFUNCTION(BlueprintCallable, meta = (NeatDelegateFunction))
void MyFunction(FMyDelegate Delegate)
{
	Delegate.ExecuteIfBound();
}

// This will show *only* the Neat version in the BP context menu
UFUNCTION(BlueprintCallable, meta = (NeatDelegateFunction, BlueprintInternalUseOnly = true))
void MyFunction2(FMyDelegate Delegate)
{
	Delegate.ExecuteIfBound();
}
```

### Parameters
More complex delegate types are supported, so you can also use delegates with parameters. Those parameters will also show up on the node.

![A node showing a delegate execute pin, along with its parameters.](Documentation/Example_02.png)
```c++
DECLARE_DYNAMIC_DELEGATE_TwoParams(FMyDelegateParams, float, FloatValue, int32, IntValue);

UFUNCTION(BlueprintCallable, meta = (NeatDelegateFunction, BlueprintInternalUseOnly = true))
void MyFunctionWithParams(FMyDelegateParams Delegate)
{
    Delegate.ExecuteIfBound(42.0f, 42);
}
```

### Multiple delegates
You can even use _different_ delegate types with different parameters on the same function, unlike async actions.

![A node shoing two different execute pins, along with its parameters.](Documentation/Example_03.png)
```c++
DECLARE_DYNAMIC_DELEGATE(FMyDelegate);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FMyDelegateParams, float, FloatValue, int32, IntValue);

UFUNCTION(BlueprintCallable, meta = (NeatDelegateFunction, BlueprintInternalUseOnly = true))
void MyFunctionMultipleDelegates(FMyDelegate Delegate, FMyDelegateParams DelegateParams)
{
    Delegate.ExecuteIfBound();
    DelegateParams.ExecuteIfBound(42.0f, 42);
}
```
