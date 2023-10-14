// Copyright Viktor Pramberg. All Rights Reserved.
#pragma once
#include "Styling/SlateStyle.h"

class FNeatFunctionsStyle final : public FSlateStyleSet
{
public:
	static const FNeatFunctionsStyle& Get()
	{
		static const FNeatFunctionsStyle Inst;
		return Inst;
	}

private:
	FNeatFunctionsStyle();
	virtual ~FNeatFunctionsStyle() override;
};
