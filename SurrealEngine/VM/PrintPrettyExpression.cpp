
#include "Precomp.h"
#include "PrintPrettyExpression.h"
#include "VM/Expression.h"
#include "VM/NativeFunc.h"
#include "VM/Frame.h"
#include "VM/Bytecode.h"
#include "Packages/Core/Properties/UProperty.h"
#include "Packages/Core/UFunction.h"

void PrintPrettyExpression::Print(std::function<void(const std::string&)> writeOutput, Expression* expr)
{
	PrintPrettyExpression builder;
	builder.writeOutput = writeOutput;

	if (expr)
	{
		expr->Visit(&builder);
	}
	else
	{
		writeOutput("Null");
	}
}

void PrintPrettyExpression::Expr(LocalVariableExpression* expr)
{
	if (expr->Variable)
		writeOutput(expr->Variable->Name.ToString());
	else
		writeOutput("(null)");
}

void PrintPrettyExpression::Expr(InstanceVariableExpression* expr)
{
	if (expr->Variable)
		writeOutput("" + expr->Variable->Name.ToString());
	else
		writeOutput("(null)");
}

void PrintPrettyExpression::Expr(DefaultVariableExpression* expr)
{
	if (expr->Variable)
		writeOutput("default."+ expr->Variable->Name.ToString());
	else
		writeOutput("default.(null)");
}

void PrintPrettyExpression::Expr(ReturnExpression* expr)
{
	writeOutput("return");
	if (expr->Value)
	{
		writeOutput(" ");
		Print(writeOutput, expr->Value);
	}
}

void PrintPrettyExpression::Expr(SwitchExpression* expr)
{
	writeOutput("switch (");
	Print(writeOutput, expr->Condition);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(JumpExpression* expr)
{
	writeOutput("jump");
}

void PrintPrettyExpression::Expr(JumpIfNotExpression* expr)
{
	writeOutput("if (");
	Print(writeOutput, expr->Condition);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(StopExpression* expr)
{
	writeOutput("stop");
}

void PrintPrettyExpression::Expr(AssertExpression* expr)
{
	writeOutput("assert(");
	Print(writeOutput, expr->Condition);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(CaseExpression* expr)
{
	writeOutput("case ");
	Print(writeOutput, expr->Value);
	writeOutput(":");
}

void PrintPrettyExpression::Expr(NothingExpression* expr)
{
	writeOutput("nothing");
}

void PrintPrettyExpression::Expr(LabelTableExpression* expr)
{
	writeOutput("label table");
}

void PrintPrettyExpression::Expr(GotoLabelExpression* expr)
{
	writeOutput("goto ");
	Print(writeOutput, expr->Value);
}

void PrintPrettyExpression::Expr(EatStringExpression* expr)
{
	writeOutput("eat string(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(LetExpression* expr)
{
	Print(writeOutput, expr->LeftSide);
	writeOutput(" = ");
	Print(writeOutput, expr->RightSide);
}

void PrintPrettyExpression::Expr(DynArrayElementExpression* expr)
{
	Print(writeOutput, expr->Array);
	writeOutput("[");
	Print(writeOutput, expr->Index);
	writeOutput("]");
}

void PrintPrettyExpression::Expr(NewExpression* expr)
{
	writeOutput("new(parent: ");
	Print(writeOutput, expr->ParentExpr);
	writeOutput(", name: ");
	Print(writeOutput, expr->NameExpr);
	writeOutput(", flags: ");
	Print(writeOutput, expr->FlagsExpr);
	writeOutput(", class: ");
	Print(writeOutput, expr->ClassExpr);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(ClassContextExpression* expr)
{
	writeOutput("class<");
	Print(writeOutput, expr->ObjectExpr);
	writeOutput(".");
	Print(writeOutput, expr->ContextExpr);
	writeOutput(">");
}

void PrintPrettyExpression::Expr(MetaCastExpression* expr)
{
	writeOutput("meta cast(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(LetBoolExpression* expr)
{
	Print(writeOutput, expr->LeftSide);
	writeOutput(" = ");
	Print(writeOutput, expr->RightSide);
}

void PrintPrettyExpression::Expr(Unknown0x15Expression* expr)
{
	writeOutput("unknown(0x15)");
}

void PrintPrettyExpression::Expr(SelfExpression* expr)
{
	writeOutput("this");
}

void PrintPrettyExpression::Expr(SkipExpression* expr)
{
	writeOutput("skip(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(ContextExpression* expr)
{
	Print(writeOutput, expr->ObjectExpr);
	writeOutput(".");
	Print(writeOutput, expr->ContextExpr);
}

void PrintPrettyExpression::Expr(ArrayElementExpression* expr)
{
	Print(writeOutput, expr->Array);
	writeOutput("[");
	Print(writeOutput, expr->Index);
	writeOutput("]");
}

void PrintPrettyExpression::Expr(IntConstExpression* expr)
{
	writeOutput(std::to_string(expr->Value));
}

void PrintPrettyExpression::Expr(FloatConstExpression* expr)
{
	writeOutput(std::to_string(expr->Value));
}

void PrintPrettyExpression::Expr(StringConstExpression* expr)
{
	writeOutput("\"" + expr->Value + "\"");
}

void PrintPrettyExpression::Expr(ObjectConstExpression* expr)
{
	if (expr->Object)
		writeOutput("{ name=\"" + expr->Object->Name.ToString() + "\", class=" + UObject::GetUClassName(expr->Object).ToString() + " }");
	else
		writeOutput("null");
}

void PrintPrettyExpression::Expr(NameConstExpression* expr)
{
	writeOutput(expr->Value.ToString());
}

void PrintPrettyExpression::Expr(RotationConstExpression* expr)
{
	writeOutput("{ " + std::to_string(expr->Pitch) + ", " + std::to_string(expr->Yaw) + ", " + std::to_string(expr->Roll) + " }");
}

void PrintPrettyExpression::Expr(VectorConstExpression* expr)
{
	writeOutput("{ " + std::to_string(expr->X) + ", " + std::to_string(expr->Y) + ", " + std::to_string(expr->Z) + " }");
}

void PrintPrettyExpression::Expr(ByteConstExpression* expr)
{
	writeOutput(std::to_string(expr->Value));
}

void PrintPrettyExpression::Expr(IntZeroExpression* expr)
{
	writeOutput("0");
}

void PrintPrettyExpression::Expr(IntOneExpression* expr)
{
	writeOutput("1");
}

void PrintPrettyExpression::Expr(TrueExpression* expr)
{
	writeOutput("true");
}

void PrintPrettyExpression::Expr(FalseExpression* expr)
{
	writeOutput("false");
}

void PrintPrettyExpression::Expr(NativeParmExpression* expr)
{
	if (expr->Object)
		writeOutput(expr->Object->Name.ToString());
	else
		writeOutput("(null)");
}

void PrintPrettyExpression::Expr(NoObjectExpression* expr)
{
	writeOutput("null");
}

void PrintPrettyExpression::Expr(Unknown0x2bExpression* expr)
{
	writeOutput("unknown(0x2b, ");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(IntConstByteExpression* expr)
{
	writeOutput(std::to_string(expr->Value));
}

void PrintPrettyExpression::Expr(BoolVariableExpression* expr)
{
	Print(writeOutput, expr->Variable);
}

void PrintPrettyExpression::Expr(DynamicCastExpression* expr)
{
	writeOutput("dynamic cast(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(IteratorExpression* expr)
{
	writeOutput("iterator(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(IteratorPopExpression* expr)
{
	writeOutput("iterator pop");
}

void PrintPrettyExpression::Expr(IteratorNextExpression* expr)
{
	writeOutput("iterator next");
}

void PrintPrettyExpression::Expr(StructCmpEqExpression* expr)
{
	Print(writeOutput, expr->Value1);
	writeOutput(" == ");
	Print(writeOutput, expr->Value2);
}

void PrintPrettyExpression::Expr(StructCmpNeExpression* expr)
{
	Print(writeOutput, expr->Value1);
	writeOutput(" != ");
	Print(writeOutput, expr->Value2);
}

void PrintPrettyExpression::Expr(UnicodeStringConstExpression* expr)
{
	writeOutput("unicode string");
}

void PrintPrettyExpression::Expr(StructMemberExpression* expr)
{
	Print(writeOutput, expr->Value);
	writeOutput(".");
	writeOutput(expr->Field->Name.ToString());
}

void PrintPrettyExpression::Expr(RotatorToVectorExpression* expr)
{
	writeOutput("rotatorToVector(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(ByteToIntExpression* expr)
{
	writeOutput("byteToInt(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(ByteToBoolExpression* expr)
{
	writeOutput("byteToBool(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(ByteToFloatExpression* expr)
{
	writeOutput("byteToFloat(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(IntToByteExpression* expr)
{
	writeOutput("intToByte(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(IntToBoolExpression* expr)
{
	writeOutput("intToBool(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(IntToFloatExpression* expr)
{
	writeOutput("intToFloat(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(BoolToByteExpression* expr)
{
	writeOutput("boolToByte(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(BoolToIntExpression* expr)
{
	writeOutput("boolToInt(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(BoolToFloatExpression* expr)
{
	writeOutput("boolToFloat(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(FloatToByteExpression* expr)
{
	writeOutput("floatToByte(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(FloatToIntExpression* expr)
{
	writeOutput("floatToInt(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(FloatToBoolExpression* expr)
{
	writeOutput("floatToBool(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(Unknown0x46Expression* expr)
{
	writeOutput("unknown(0x46, ");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(ObjectToBoolExpression* expr)
{
	writeOutput("objectToBool(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(NameToBoolExpression* expr)
{
	writeOutput("nameToBool(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(StringToByteExpression* expr)
{
	writeOutput("stringToByte(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(StringToIntExpression* expr)
{
	writeOutput("stringToInt(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(StringToBoolExpression* expr)
{
	writeOutput("stringToBool(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(StringToFloatExpression* expr)
{
	writeOutput("stringToFloat(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(StringToVectorExpression* expr)
{
	writeOutput("stringToVector(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(StringToRotatorExpression* expr)
{
	writeOutput("stringToRotator(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(VectorToBoolExpression* expr)
{
	writeOutput("vectorToBool(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(VectorToRotatorExpression* expr)
{
	writeOutput("vectorToRotator(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(RotatorToBoolExpression* expr)
{
	writeOutput("rotatorToBool(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(ByteToStringExpression* expr)
{
	writeOutput("byteToString(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(IntToStringExpression* expr)
{
	writeOutput("intToString(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(BoolToStringExpression* expr)
{
	writeOutput("boolToString(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(FloatToStringExpression* expr)
{
	writeOutput("floatToString(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(ObjectToStringExpression* expr)
{
	writeOutput("objectToString(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(NameToStringExpression* expr)
{
	writeOutput("nameToString(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(VectorToStringExpression* expr)
{
	writeOutput("vectorToString(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(RotatorToStringExpression* expr)
{
	writeOutput("rotatorToString(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(StringToNameExpression* expr)
{
	writeOutput("stringToName(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(DynArrayToIntExpression* expr)
{
	writeOutput("dynarrayToInt(");
	Print(writeOutput, expr->Value);
	writeOutput(")");
}

void PrintPrettyExpression::Expr(VirtualFunctionExpression* expr)
{
	writeOutput(expr->Name.ToString() + "(");
	int index = 0;
	for (auto arg : expr->Args)
	{
		if (index != 0)
			writeOutput(", ");
		Print(writeOutput, arg);
		index++;
	}
	writeOutput(")");
}

void PrintPrettyExpression::Expr(FinalFunctionExpression* expr)
{
	writeOutput(expr->Func->Name.ToString() + "(");
	int index = 0;
	for (auto arg : expr->Args)
	{
		if (index != 0)
			writeOutput(", ");
		Print(writeOutput, arg);
		index++;
	}
	writeOutput(")");
}

void PrintPrettyExpression::Expr(GlobalFunctionExpression* expr)
{
	writeOutput(expr->Name.ToString() + "(");
	int index = 0;
	for (auto arg : expr->Args)
	{
		if (index != 0)
			writeOutput(", ");
		Print(writeOutput, arg);
		index++;
	}
	writeOutput(")");
}

void PrintPrettyExpression::Expr(NativeFunctionExpression* expr)
{
	if (NativeFunctions::FuncByIndex[expr->nativeindex])
		writeOutput(NativeFunctions::FuncByIndex[expr->nativeindex]->Name.ToString() + "(");
	else
		writeOutput("UnknownNative" + std::to_string(expr->nativeindex) + "(");
	int index = 0;
	for (auto arg : expr->Args)
	{
		if (index != 0)
			writeOutput(", ");
		Print(writeOutput, arg);
		index++;
	}
	writeOutput(")");
}

void PrintPrettyExpression::Expr(FunctionArgumentsExpression* expr)
{
	writeOutput("function arguments");
}

void PrintPrettyExpression::Expr(ConstructExpression* expr)
{
	writeOutput("construct " + expr->Struct->Name.ToString() + "(");
	int index = 0;
	for (auto arg : expr->Args)
	{
		if (index != 0)
			writeOutput(", ");
		if (arg.Name)
			writeOutput(arg.Name->Name.ToString() + ": ");
		Print(writeOutput, arg.Value);
		index++;
	}
	writeOutput(")");
}
