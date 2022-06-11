#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <popl.hpp>

#include <wasm-binary.h>
#include <wasm-features.h>

std::string indentation = "";

size_t expressionDepth = 0;

wasm::Module *ParseWasm(const std::vector<char> &binaryData)
{
    wasm::Module *module = new wasm::Module;
    wasm::WasmBinaryBuilder parser(*module, FeatureSet::MVP, binaryData);
    parser.read();

    return module;
}
std::string GetStringFromWasmType(const wasm::Type &type)
{
    std::string stringType;

    if (type.getBasic() == wasm::Type::BasicType::i32)
        stringType = "int32_t";
    else if (type.getBasic() == wasm::Type::BasicType::f32)
        stringType = "float";
    else if (type.getBasic() == wasm::Type::BasicType::f64)
        stringType = "double";
    else if (type.getBasic() == wasm::Type::BasicType::i64)
        stringType = "int64_t";
    else if (type.getBasic() == wasm::Type::BasicType::none)
        stringType = "void";
    else
        std::cout << "cannot convert type " << std::to_string(type.getBasic()) << " to string" << std::endl;

    return stringType;
}
std::string GetFunctionSignature(const std::unique_ptr<wasm::Function> &function)
{
    std::string output;

    const wasm::Signature &signature = function->getSig();
    output += GetStringFromWasmType(signature.results);

    output += " func";
    output += function->name.str;
    output += "(";

    size_t index = 0;
    for (auto i = signature.params.begin(); i != signature.params.end(); i++)
    {
        const wasm::Type parameterType = signature.params[index];
        output += GetStringFromWasmType(parameterType);
        output += " v";
        output += std::to_string(index);
        if (i != signature.params.end() - 1)
            output += ", ";
        index++;
    }
    output += ")";

    return output;
}
void GetWasm2cExperssion(std::string &output, wasm::Expression *expression, size_t depth)
{
    wasm::Expression::Id id = expression->_id;
    if (id == wasm::Expression::CallId)
    {
        wasm::Call *functionCall = static_cast<wasm::Call *>(expression);
        if (expressionDepth == 0)
            output += indentation;
        output += "func";
        output += functionCall->target.str;
        output += "(";
        expressionDepth++;
        size_t i = 0;
        for (wasm::Expression *operand : functionCall->operands)
        {
            GetWasm2cExperssion(output, operand, depth + 1);
            if (i != functionCall->operands.size() - 1)
                output += ", ";
            i++;
        }
        output += ")";
        expressionDepth--;
        if (expressionDepth == 0)
            output += ";\n";
    }
    else if (id == wasm::Expression::BlockId)
    {
        wasm::Block *block = static_cast<wasm::Block *>(expression);
        if (block == nullptr)
            return;
        if (expressionDepth == 0)
            output += indentation;
        if (block->name.str != nullptr)
            output += std::string(block->name.str) + ":\n" + indentation;
        output += "{\n";
        indentation += "    ";
        size_t _expressionDepth = expressionDepth;
        expressionDepth = 0;
        for (wasm::Expression *expression : block->list)
        {
            GetWasm2cExperssion(output, expression, depth + 1);
        }
        expressionDepth = _expressionDepth;
        indentation = indentation.substr(4);
        output += indentation + "}";
        if (expressionDepth == 0)
            output += "\n";
    }
    else if (id == wasm::Expression::LocalGetId)
    {
        wasm::LocalGet *localGetExpression = static_cast<wasm::LocalGet *>(expression);

        if (expressionDepth != 0)
            output += "v" + std::to_string(localGetExpression->index);
        else
            output += indentation + "v" + std::to_string(localGetExpression->index) + ";\n";
    }
    else if (id == wasm::Expression::LoadId)
    {
        wasm::Load *loadInstruction = static_cast<wasm::Load *>(expression);

        if (expressionDepth == 0)
            output += indentation;

        if (loadInstruction->bytes == 4)
        {

            output += std::string("u32[(");
            expressionDepth++;
            GetWasm2cExperssion(output, loadInstruction->ptr, depth + 1);
            expressionDepth--;
            output += " + " + std::to_string(loadInstruction->offset) + ") >> 2]";
        }
        else
        {
            output += "unimplementedload" + std::to_string(loadInstruction->bytes);
        }
        if (expressionDepth == 0)
            output += ";\n";
    }
    else if (id == wasm::Expression::LocalSetId)
    {
        wasm::LocalSet *setInstruction = static_cast<wasm::LocalSet *>(expression);

        if (expressionDepth == 0)
            output += indentation;
        output += "v" + std::to_string(setInstruction->index) + " = ";
        expressionDepth++;
        GetWasm2cExperssion(output, setInstruction->value, depth + 1);
        expressionDepth--;
        if (expressionDepth == 0)
            output += ";\n";
    }
    else if (id == wasm::Expression::BreakId)
    {
        wasm::Break *breakInstruction = static_cast<wasm::Break *>(expression);

        std::string label = breakInstruction->name.str != nullptr ? breakInstruction->name.str : "";

        if (breakInstruction->condition != nullptr)
        {
            if (expressionDepth == 0)
                output += indentation;

            output += "if (";
            expressionDepth++;
            GetWasm2cExperssion(output, breakInstruction->condition, depth + 1);
            expressionDepth--;
            output += ")\n";
            indentation += "    ";
        }
        output += indentation + "break";
        if (breakInstruction->condition != nullptr)
            indentation = indentation.substr(4);
        if (breakInstruction->value != nullptr)
        {
            output += " (";
            expressionDepth++;
            GetWasm2cExperssion(output, breakInstruction->value, depth + 1);
            expressionDepth--;
            output += ")";
        }
        output += ";\n";
    }
    else if (id == wasm::Expression::UnaryId)
    {
        wasm::Unary *unaryInstruction = static_cast<wasm::Unary *>(expression);
#define APPEND_TO_OUTPUT(x)                                              \
    else if (unaryInstruction->op == wasm::x)                            \
    {                                                                    \
        output += std::string("__") + #x + "(";                          \
        expressionDepth++;                                               \
        GetWasm2cExperssion(output, unaryInstruction->value, depth + 1); \
        expressionDepth--;                                               \
        output += ")";                                                   \
    }

        if (false)
        {
        }
        APPEND_TO_OUTPUT(ClzInt32)
        APPEND_TO_OUTPUT(ClzInt64)
        APPEND_TO_OUTPUT(CtzInt32)
        APPEND_TO_OUTPUT(CtzInt64)
        APPEND_TO_OUTPUT(PopcntInt32)
        APPEND_TO_OUTPUT(PopcntInt64)
        APPEND_TO_OUTPUT(NegFloat32)
        APPEND_TO_OUTPUT(NegFloat64)
        APPEND_TO_OUTPUT(AbsFloat32)
        APPEND_TO_OUTPUT(AbsFloat64)
        APPEND_TO_OUTPUT(CeilFloat32)
        APPEND_TO_OUTPUT(CeilFloat64)
        APPEND_TO_OUTPUT(FloorFloat32)
        APPEND_TO_OUTPUT(FloorFloat64)
        APPEND_TO_OUTPUT(TruncFloat32)
        APPEND_TO_OUTPUT(TruncFloat64)
        APPEND_TO_OUTPUT(NearestFloat32)
        APPEND_TO_OUTPUT(NearestFloat64)
        APPEND_TO_OUTPUT(SqrtFloat32)
        APPEND_TO_OUTPUT(SqrtFloat64)
        APPEND_TO_OUTPUT(EqZInt32)
        APPEND_TO_OUTPUT(EqZInt64)
        APPEND_TO_OUTPUT(ExtendSInt32)
        APPEND_TO_OUTPUT(ExtendUInt32)
        APPEND_TO_OUTPUT(WrapInt64)
        APPEND_TO_OUTPUT(TruncSFloat32ToInt32)
        APPEND_TO_OUTPUT(TruncSFloat32ToInt64)
        APPEND_TO_OUTPUT(TruncUFloat32ToInt32)
        APPEND_TO_OUTPUT(TruncUFloat32ToInt64)
        APPEND_TO_OUTPUT(TruncSFloat64ToInt32)
        APPEND_TO_OUTPUT(TruncSFloat64ToInt64)
        APPEND_TO_OUTPUT(TruncUFloat64ToInt32)
        APPEND_TO_OUTPUT(TruncUFloat64ToInt64)
        APPEND_TO_OUTPUT(ReinterpretFloat32)
        APPEND_TO_OUTPUT(ReinterpretFloat64)
        APPEND_TO_OUTPUT(ConvertSInt32ToFloat32)
        APPEND_TO_OUTPUT(ConvertSInt32ToFloat64)
        APPEND_TO_OUTPUT(ConvertUInt32ToFloat32)
        APPEND_TO_OUTPUT(ConvertUInt32ToFloat64)
        APPEND_TO_OUTPUT(ConvertSInt64ToFloat32)
        APPEND_TO_OUTPUT(ConvertSInt64ToFloat64)
        APPEND_TO_OUTPUT(ConvertUInt64ToFloat32)
        APPEND_TO_OUTPUT(ConvertUInt64ToFloat64)
        APPEND_TO_OUTPUT(PromoteFloat32)
        APPEND_TO_OUTPUT(DemoteFloat64)
        APPEND_TO_OUTPUT(ReinterpretInt32)
        APPEND_TO_OUTPUT(ReinterpretInt64)
        APPEND_TO_OUTPUT(ExtendS8Int32)
        APPEND_TO_OUTPUT(ExtendS16Int32)
        APPEND_TO_OUTPUT(ExtendS8Int64)
        APPEND_TO_OUTPUT(ExtendS16Int64)
        APPEND_TO_OUTPUT(ExtendS32Int64)
        APPEND_TO_OUTPUT(TruncSatSFloat32ToInt32)
        APPEND_TO_OUTPUT(TruncSatUFloat32ToInt32)
        APPEND_TO_OUTPUT(TruncSatSFloat64ToInt32)
        APPEND_TO_OUTPUT(TruncSatUFloat64ToInt32)
        APPEND_TO_OUTPUT(TruncSatSFloat32ToInt64)
        APPEND_TO_OUTPUT(TruncSatUFloat32ToInt64)
        APPEND_TO_OUTPUT(TruncSatSFloat64ToInt64)
        APPEND_TO_OUTPUT(TruncSatUFloat64ToInt64)
        APPEND_TO_OUTPUT(SplatVecI8x16)
        APPEND_TO_OUTPUT(SplatVecI16x8)
        APPEND_TO_OUTPUT(SplatVecI32x4)
        APPEND_TO_OUTPUT(SplatVecI64x2)
        APPEND_TO_OUTPUT(SplatVecF32x4)
        APPEND_TO_OUTPUT(SplatVecF64x2)
        APPEND_TO_OUTPUT(NotVec128)
        APPEND_TO_OUTPUT(AnyTrueVec128)
        APPEND_TO_OUTPUT(AbsVecI8x16)
        APPEND_TO_OUTPUT(NegVecI8x16)
        APPEND_TO_OUTPUT(AllTrueVecI8x16)
        APPEND_TO_OUTPUT(BitmaskVecI8x16)
        APPEND_TO_OUTPUT(PopcntVecI8x16)
        APPEND_TO_OUTPUT(AbsVecI16x8)
        APPEND_TO_OUTPUT(NegVecI16x8)
        APPEND_TO_OUTPUT(AllTrueVecI16x8)
        APPEND_TO_OUTPUT(BitmaskVecI16x8)
        APPEND_TO_OUTPUT(AbsVecI32x4)
        APPEND_TO_OUTPUT(NegVecI32x4)
        APPEND_TO_OUTPUT(AllTrueVecI32x4)
        APPEND_TO_OUTPUT(BitmaskVecI32x4)
        APPEND_TO_OUTPUT(AbsVecI64x2)
        APPEND_TO_OUTPUT(NegVecI64x2)
        APPEND_TO_OUTPUT(AllTrueVecI64x2)
        APPEND_TO_OUTPUT(BitmaskVecI64x2)
        APPEND_TO_OUTPUT(AbsVecF32x4)
        APPEND_TO_OUTPUT(NegVecF32x4)
        APPEND_TO_OUTPUT(SqrtVecF32x4)
        APPEND_TO_OUTPUT(CeilVecF32x4)
        APPEND_TO_OUTPUT(FloorVecF32x4)
        APPEND_TO_OUTPUT(TruncVecF32x4)
        APPEND_TO_OUTPUT(NearestVecF32x4)
        APPEND_TO_OUTPUT(AbsVecF64x2)
        APPEND_TO_OUTPUT(NegVecF64x2)
        APPEND_TO_OUTPUT(SqrtVecF64x2)
        APPEND_TO_OUTPUT(CeilVecF64x2)
        APPEND_TO_OUTPUT(FloorVecF64x2)
        APPEND_TO_OUTPUT(TruncVecF64x2)
        APPEND_TO_OUTPUT(NearestVecF64x2)
        APPEND_TO_OUTPUT(ExtAddPairwiseSVecI8x16ToI16x8)
        APPEND_TO_OUTPUT(ExtAddPairwiseUVecI8x16ToI16x8)
        APPEND_TO_OUTPUT(ExtAddPairwiseSVecI16x8ToI32x4)
        APPEND_TO_OUTPUT(ExtAddPairwiseUVecI16x8ToI32x4)
        APPEND_TO_OUTPUT(TruncSatSVecF32x4ToVecI32x4)
        APPEND_TO_OUTPUT(TruncSatUVecF32x4ToVecI32x4)
        APPEND_TO_OUTPUT(ConvertSVecI32x4ToVecF32x4)
        APPEND_TO_OUTPUT(ConvertUVecI32x4ToVecF32x4)
        APPEND_TO_OUTPUT(ExtendLowSVecI8x16ToVecI16x8)
        APPEND_TO_OUTPUT(ExtendHighSVecI8x16ToVecI16x8)
        APPEND_TO_OUTPUT(ExtendLowUVecI8x16ToVecI16x8)
        APPEND_TO_OUTPUT(ExtendHighUVecI8x16ToVecI16x8)
        APPEND_TO_OUTPUT(ExtendLowSVecI16x8ToVecI32x4)
        APPEND_TO_OUTPUT(ExtendHighSVecI16x8ToVecI32x4)
        APPEND_TO_OUTPUT(ExtendLowUVecI16x8ToVecI32x4)
        APPEND_TO_OUTPUT(ExtendHighUVecI16x8ToVecI32x4)
        APPEND_TO_OUTPUT(ExtendLowSVecI32x4ToVecI64x2)
        APPEND_TO_OUTPUT(ExtendHighSVecI32x4ToVecI64x2)
        APPEND_TO_OUTPUT(ExtendLowUVecI32x4ToVecI64x2)
        APPEND_TO_OUTPUT(ExtendHighUVecI32x4ToVecI64x2)
        APPEND_TO_OUTPUT(ConvertLowSVecI32x4ToVecF64x2)
        APPEND_TO_OUTPUT(ConvertLowUVecI32x4ToVecF64x2)
        APPEND_TO_OUTPUT(TruncSatZeroSVecF64x2ToVecI32x4)
        APPEND_TO_OUTPUT(TruncSatZeroUVecF64x2ToVecI32x4)
        APPEND_TO_OUTPUT(DemoteZeroVecF64x2ToVecF32x4)
        APPEND_TO_OUTPUT(PromoteLowVecF32x4ToVecF64x2)
        APPEND_TO_OUTPUT(RelaxedTruncSVecF32x4ToVecI32x4)
        APPEND_TO_OUTPUT(RelaxedTruncUVecF32x4ToVecI32x4)
        APPEND_TO_OUTPUT(RelaxedTruncZeroSVecF64x2ToVecI32x4)
        APPEND_TO_OUTPUT(RelaxedTruncZeroUVecF64x2ToVecI32x4)
        APPEND_TO_OUTPUT(InvalidUnary)
    }
    else if (id == wasm::Expression::UnreachableId)
    {
        if (expressionDepth == 0)
            output += indentation;
        output += "assert(false)";
        if (expressionDepth == 0)
            output += ";\n";
    }
    else if (id == wasm::Expression::BinaryId)
    {
        wasm::Binary *instruction = static_cast<wasm::Binary *>(expression);

        if (instruction->left->_id == wasm::Expression::BinaryId || instruction->left->_id == wasm::Expression::UnaryId || instruction->left->_id == wasm::Expression::LocalSetId)
            output += "(";
        expressionDepth++;
        GetWasm2cExperssion(output, instruction->left, depth + 1);
        expressionDepth--;
        if (instruction->left->_id == wasm::Expression::BinaryId || instruction->left->_id == wasm::Expression::UnaryId || instruction->left->_id == wasm::Expression::LocalSetId)
            output += ")";

        if (instruction->op == wasm::AddFloat32 || instruction->op == wasm::AddFloat64 || instruction->op == wasm::AddInt32 || instruction->op == wasm::AddInt64 || instruction->op == wasm::AddSatSVecI16x8 || instruction->op == wasm::AddSatSVecI8x16 || instruction->op == wasm::AddSatUVecI16x8 || instruction->op == wasm::AddSatUVecI8x16 || instruction->op == wasm::AddVecF32x4 || instruction->op == wasm::AddVecF64x2 || instruction->op == wasm::AddVecI16x8 || instruction->op == wasm::AddVecI32x4 || instruction->op == wasm::AddVecI64x2 || instruction->op == wasm::AddVecI8x16)
            output += " + ";
        else if (instruction->op == wasm::SubFloat32 || instruction->op == wasm::SubFloat64 || instruction->op == wasm::SubInt32 || instruction->op == wasm::SubInt64 || instruction->op == wasm::SubSatSVecI16x8 || instruction->op == wasm::SubSatSVecI8x16 || instruction->op == wasm::SubSatUVecI16x8 || instruction->op == wasm::SubSatUVecI8x16 || instruction->op == wasm::SubVecF32x4 || instruction->op == wasm::SubVecF64x2 || instruction->op == wasm::SubVecI16x8 || instruction->op == wasm::SubVecI32x4 || instruction->op == wasm::SubVecI64x2 || instruction->op == wasm::SubVecI8x16)
            output += " - ";
        else if (instruction->op == wasm::AndInt32 || instruction->op == wasm::AndInt64 || instruction->op == wasm::AndNotVec128 || instruction->op == wasm::AndVec128)
            output += " & ";
        else if (instruction->op == wasm::LtFloat32 || instruction->op == wasm::LtFloat64 || instruction->op == wasm::LtSInt32 || instruction->op == wasm::LtSInt64 || instruction->op == wasm::LtSVecI16x8 || instruction->op == wasm::LtSVecI32x4 || instruction->op == wasm::LtSVecI64x2 || instruction->op == wasm::LtSVecI8x16 || instruction->op == wasm::LtUInt32 || instruction->op == wasm::LtUInt64 || instruction->op == wasm::LtUVecI16x8 || instruction->op == wasm::LtUVecI32x4 || instruction->op == wasm::LtUVecI8x16 || instruction->op == wasm::LtVecF32x4 || instruction->op == wasm::LtVecF64x2)
            output += " < ";
        else if (instruction->op == wasm::NeInt32 || instruction->op == wasm::NeFloat32 || instruction->op == wasm::NeFloat64 || instruction->op == wasm::NeInt64 || instruction->op == wasm::NeVecF32x4 || instruction->op == wasm::NeVecF64x2 || instruction->op == wasm::NeVecI16x8 || instruction->op == wasm::NeVecI32x4 || instruction->op == wasm::NeVecI64x2 || instruction->op == wasm::NeVecI8x16)
            output += " != ";
        else if (instruction->op == wasm::LeFloat32 || instruction->op == wasm::LeFloat64 || instruction->op == wasm::LeSInt32 || instruction->op == wasm::LeSInt64 || instruction->op == wasm::LeSVecI16x8 || instruction->op == wasm::LeSVecI32x4 || instruction->op == wasm::LeSVecI64x2 || instruction->op == wasm::LeSVecI8x16 || instruction->op == wasm::LeUInt32 || instruction->op == wasm::LeUInt64 || instruction->op == wasm::LeUVecI16x8 || instruction->op == wasm::LeUVecI32x4 || instruction->op == wasm::LeUVecI8x16 || instruction->op == wasm::LeVecF32x4 || instruction->op == wasm::LeVecF64x2)
            output += " <= ";
        else if (instruction->op == wasm::RemSInt32 || instruction->op == wasm::RemSInt64 || instruction->op == wasm::RemUInt32 || instruction->op == wasm::RemUInt64)
            output += " % ";
        else if (instruction->op == wasm::XorInt32 || instruction->op == wasm::XorInt64 || instruction->op == wasm::XorVec128)
            output += " ^ ";
        else if (instruction->op == wasm::ShlInt32 || instruction->op == wasm::ShlInt64)
            output += " << ";
        else if (instruction->op == wasm::ShrSInt32 || instruction->op == wasm::ShrSInt64 || instruction->op == wasm::ShrUInt32 || instruction->op == wasm::ShrUInt64)
            output += " >> ";
        else if (instruction->op == wasm::EqFloat32 || instruction->op == wasm::EqFloat64 || instruction->op == wasm::EqInt32 || instruction->op == wasm::EqInt64 || instruction->op == wasm::EqVecF32x4 || instruction->op == wasm::EqVecF64x2 || instruction->op == wasm::EqVecI16x8 || instruction->op == wasm::EqVecI32x4 || instruction->op == wasm::EqVecI64x2 || instruction->op == wasm::EqVecI8x16)
            output += " == ";
        else if (instruction->op == wasm::RotLInt32 || instruction->op == wasm::RotLInt64)
            output += " <<< ";
        else if (instruction->op == wasm::RotRInt32 || instruction->op == wasm::RotRInt64)
            output += " >>> ";
        else
            output += " #" + std::to_string(instruction->op) + " ";

        if (instruction->right->_id == wasm::Expression::BinaryId || instruction->right->_id == wasm::Expression::UnaryId || instruction->right->_id == wasm::Expression::LocalSetId)
            output += "(";
        expressionDepth++;
        GetWasm2cExperssion(output, instruction->right, depth + 1);
        expressionDepth--;
        if (instruction->right->_id == wasm::Expression::BinaryId || instruction->right->_id == wasm::Expression::UnaryId || instruction->right->_id == wasm::Expression::LocalSetId)
            output += ")";

        if (expressionDepth == 0)
            output += ";\n";
    }
    else if (id == wasm::Expression::StoreId)
    {
        wasm::Store *instruction = static_cast<wasm::Store *>(expression);

        if (expressionDepth == 0)
            output += indentation;

        if (instruction->bytes == 4)
        {

            output += std::string("u32[(");
            expressionDepth++;
            GetWasm2cExperssion(output, instruction->ptr, depth + 1);
            expressionDepth--;
            output += " + " + std::to_string(instruction->offset) + ") >> 2]";
        }
        else
        {
            output += "unimplementedstore" + std::to_string(instruction->bytes);
        }

        output += " = ";

        expressionDepth++;
        GetWasm2cExperssion(output, instruction->value, depth + 1);
        expressionDepth--;

        if (expressionDepth == 0)
            output += ";\n";
    }
    else if (id == wasm::Expression::ConstId)
    {
        wasm::Const *instruction = static_cast<wasm::Const *>(expression);

        if (instruction->type == wasm::Type::f32)
            output += std::to_string(instruction->value.getf32());
        else if (instruction->type == wasm::Type::i32)
            output += std::to_string(instruction->value.geti32());
        else
            output += "const" + std::to_string(instruction->type.getID());
    }
    else if (id == wasm::Expression::IfId)
    {
        wasm::If *instruction = static_cast<wasm::If *>(expression);

        if (expressionDepth == 0)
            output += indentation;

        output += "if (";
        expressionDepth++;
        GetWasm2cExperssion(output, instruction->condition, depth + 1);
        expressionDepth--;
        output += ")\n";

        if (instruction->ifTrue->_id != wasm::Expression::BlockId)
            indentation += "    ";
        size_t _expressionDepth = expressionDepth;
        expressionDepth = 0;
        GetWasm2cExperssion(output, instruction->ifTrue, depth + 1);
        expressionDepth = _expressionDepth;
        if (instruction->ifTrue->_id != wasm::Expression::BlockId)
            indentation = indentation.substr(4);

        if (instruction->ifFalse != nullptr)
        {
            output += "\n" + indentation + "else\n";
            if (instruction->ifFalse->_id != wasm::Expression::BlockId)
                indentation += "    ";

            _expressionDepth = expressionDepth;
            expressionDepth = 0;
            GetWasm2cExperssion(output, instruction->ifFalse, depth + 1);
            expressionDepth = _expressionDepth;
            if (instruction->ifFalse->_id != wasm::Expression::BlockId)
                indentation = indentation.substr(4);
        }
    }
    else if (id == wasm::Expression::DropId)
    {
        wasm::Drop *instruction = static_cast<wasm::Drop *>(expression);

        if (expressionDepth == 0)
            output += indentation;
        output += "drop(";
        expressionDepth++;
        GetWasm2cExperssion(output, instruction->value, depth + 1);
        expressionDepth--;
        output += ");\n";
    }
    else if (id == wasm::Expression::SwitchId)
    {
        wasm::Switch *instruction = static_cast<wasm::Switch *>(expression);

        output += indentation + "switch(";
        expressionDepth++;
        GetWasm2cExperssion(output, instruction->condition, depth + 1);
        expressionDepth--;
        output += ")\n" + indentation + "{";
        indentation += "    ";
        for (size_t i = 0; i < instruction->targets.size(); i++)
        {
            output += indentation + "case " + std::to_string(i) + ": break " + instruction->targets[i].str + ";\n";
        }
        indentation = indentation.substr(4);

        output += indentation + "}\n";
    }
    else if (id == wasm::Expression::ReturnId)
    {
        wasm::Return *instruction = static_cast<wasm::Return *>(expression);

        output += indentation + "return";
        if (instruction->value != nullptr)
        {
            output += " ";
            expressionDepth++;
            GetWasm2cExperssion(output, instruction->value, depth + 1);
            expressionDepth--;
        }
        output += ";\n";
    }
    else if (id == wasm::Expression::GlobalSetId)
    {
        wasm::GlobalSet *instruction = static_cast<wasm::GlobalSet *>(expression);

        output += indentation + instruction->name.str + " = ";

        expressionDepth++;
        GetWasm2cExperssion(output, instruction->value, depth);
        expressionDepth--;

        output += ";\n";
    }
    else if (id == wasm::Expression::GlobalGetId)
    {
        wasm::GlobalGet *instruction = static_cast<wasm::GlobalGet *>(expression);

        if (expressionDepth == 0)
            output += indentation;

        output += instruction->name.str;

        if (expressionDepth == 0)
            output += ";\n";
    }
    else if (id == wasm::Expression::LoopId)
    {
        wasm::Loop *instruction = static_cast<wasm::Loop *>(expression);
        output += indentation + "while (true)\n";
        GetWasm2cExperssion(output, instruction->body, depth + 1);
    }
    else if (id == wasm::Expression::SelectId)
    {
        wasm::Select *instruction = static_cast<wasm::Select *>(expression);

        if (expressionDepth == 0)
            output += indentation;

        expressionDepth++;
        GetWasm2cExperssion(output, instruction->condition, depth + 1);
        expressionDepth--;

        output += " ? ";

        expressionDepth++;
        GetWasm2cExperssion(output, instruction->ifTrue, depth + 1);
        expressionDepth--;

        output += " : ";

        expressionDepth++;
        GetWasm2cExperssion(output, instruction->ifFalse, depth + 1);
        expressionDepth--;

        if (expressionDepth == 0)
            output += ";\n";
    }
    else if (id == wasm::Expression::MemorySizeId)
    {
        output += "MEMORY_SIZE";
    }
    else if (id == wasm::Expression::NopId)
    {
    }
    else if (id == wasm::Expression::CallIndirectId)
    {   
    }
    else
    {
        if (expressionDepth == 0)
            output += indentation;
        output += "unimplemented" + std::to_string(id);
        if (expressionDepth == 0)
            output += ";\n";
    }
}
std::string GetWasm2cFunctionBody(const std::unique_ptr<wasm::Function> &function)
{
    std::string output;

    if (function->body == nullptr)
        return "// imported\n";

    GetWasm2cExperssion(output, function->body, 0);

    return output;
}
std::string GetWasm2cFunctionLocals(const std::unique_ptr<wasm::Function> &function)
{
    std::string output;

    for (size_t i = function->getNumParams() - 1; i < function->getNumVars(); i++)
    {
        std::string local;

        const wasm::Type &type = function->getLocalType(i);

        local += GetStringFromWasmType(type);

        output += indentation + local + " v" + std::to_string(i + 1) + ";\n";
    }

    return output;
}
std::string GenerateWasm2cFunctionBodies(const wasm::Module *module)
{
    std::string output;
    for (const std::unique_ptr<wasm::Function> &function : module->functions)
    {
        std::string body;
        body += GetFunctionSignature(function);

        body += "\n{\n"; // open function body
        indentation += "    ";

        body += GetWasm2cFunctionLocals(function);
        body += GetWasm2cFunctionBody(function);

        indentation = indentation.substr(4);
        body += "}";

        output += body;
        output += "\n\n";
    }

    return output;
}
std::string GenerateWasm2cFunctionDeclarations(const wasm::Module *module)
{
    std::string output;
    for (const std::unique_ptr<wasm::Function> &function : module->functions)
    {
        std::string declaration;

        declaration += GetFunctionSignature(function);

        declaration += ";\n";

        output += declaration;
    }

    return output;
}
std::string GenerateWasm2cMemory(const wasm::Module *module)
{
    std::string output;
    uint64_t memorySize = module->memory.max.addr;

    output += "uint8_t *u8 = (uint8_t *)malloc(" + std::to_string(memorySize) + " << 16);\n"
                                                                                "uint16_t *u16 = (uint16_t *)u8;\n"
                                                                                "uint32_t *u32 = (uint32_t *)u8;\n"
                                                                                "uint64_t *u64 = (uint64_t *)u8;\n"
                                                                                "int8_t *i8 = (int8_t *)u8;\n"
                                                                                "int16_t *i16 = (int16_t *)u8;\n"
                                                                                "int32_t *i32 = (int32_t *)u8;\n"
                                                                                "int64_t *i64 = (int64_t *)u8;\n"
                                                                                "float *f32 = (float *)u8;\n"
                                                                                "double *f64 = (double *)u8;\n\n";
    return output;
}
std::string GenerateWasm2cGlobals(const wasm::Module *module)
{
    std::string globals;
    for (const std::unique_ptr<wasm::Global> &global : module->globals)
    {
        wasm::Const *wasmConstant = global->init->dynCast<wasm::Const>();
        std::string globalValue;
        std::string type = GetStringFromWasmType(wasmConstant->type);

        if (wasmConstant->type == wasm::Type::i32)
        {
            globalValue = std::to_string(wasmConstant->value.geti32());
        }
        else
        {
            globalValue = "unknown";
        }
        globals += type + " " + global->name.str + " = " + globalValue + ";\n";
    }

    globals += "\n";

    return globals;
}
std::string GenerateWasm2c(const wasm::Module *module)
{
    std::string output;
    output += "#include <stdint.h>\n"
              "#include <stdlib.h>\n"
              "\n";

    output += GenerateWasm2cGlobals(module);
    output += GenerateWasm2cMemory(module);
    output += GenerateWasm2cFunctionDeclarations(module);
    output += GenerateWasm2cFunctionBodies(module);

    return output;
}
void WriteOutput(const wasm::Module *module, const std::string &outputFile)
{
    std::ofstream ouputFileStream(outputFile);

    ouputFileStream << GenerateWasm2c(module);
}
std::vector<char> ReadDataFromFilePath(const std::string &path)
{
    std::ifstream fileStream(path, std::ios::binary);
    if (!fileStream.is_open())
    {
        std::cout << "could not open file path " << path << std::endl;
        throw std::runtime_error("unable to open file");
    }

    std::vector<char> fileData;

    char byte;
    while (fileStream.get(byte))
        fileData.push_back(byte);

    std::cout << "read file of " << fileData.size() << " size" << std::endl;
    return fileData;
}
int32_t main(int32_t argumentCount, char **argumentValues)
{
    popl::OptionParser commandLineParser("idk");

    std::shared_ptr<popl::Value<std::string>> inputFileOption = commandLineParser.add<popl::Value<std::string>>("i", "input", "the file to read from");
    std::shared_ptr<popl::Value<std::string>> outputFileOption = commandLineParser.add<popl::Value<std::string>>("o", "output", "the output file");
    std::string outputFile;
    std::string inputFile;

    commandLineParser.parse(argumentCount, argumentValues);

    if (outputFileOption->is_set())
        outputFile = outputFileOption->value();
    else
        outputFile = "a.c";

    if (!inputFileOption->is_set())
    {
        std::cout << "--input option not specified" << std::endl;
        return 1;
    }
    inputFile = inputFileOption->value();

    std::vector<char> data = ReadDataFromFilePath(inputFile);

    wasm::Module *module = ParseWasm(data);

    WriteOutput(module, outputFile);

    delete module;

    return 0;
}
