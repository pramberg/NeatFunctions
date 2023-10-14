# <img src="Content/Editor/FunctionIcon.svg" style="height:1.0em;" align="center"/> Neat Functions
Unreal Engine plugin that extends UFunctions in some neat ways. Functions that take delegates can have their events created in the function node itself, making them look like async actions.

## Example
This is a simple example of what this plugin does, with the code used to create the nodes below.
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