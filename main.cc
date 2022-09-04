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
std::string GetStringFromWasmType(wasm::Type &type)
{
    switch (type.getBasic())
    {
    case wasm::Type::BasicType::i32:
        return "int32_t";
    case wasm::Type::BasicType::f32:
        return "float";
    case wasm::Type::BasicType::f64:
        return "double";
    case wasm::Type::BasicType::i64:
        return "int64_t";
    case wasm::Type::BasicType::none:
        return "void";
    default:
        std::cout << "cannot convert type " << std::to_string(type.getBasic()) << " to string" << std::endl;

        return std::string("#") + std::to_string(type.getBasic());
    }
}
std::string GetFunctionSignature(wasm::Function *function)
{
    std::string output;

    wasm::Signature signature = function->getSig();
    output += GetStringFromWasmType(signature.results) + " func" + function->name.str + "(";

    size_t index = 0;
    for (auto i = signature.params.begin(); i != signature.params.end(); i++)
    {
        wasm::Type parameterType = signature.params[index];
        output += GetStringFromWasmType(parameterType) + " v" + std::to_string(index);
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
    switch (id)
    {
    case wasm::Expression::CallId:
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
            else if (operand->_id == wasm::Expression::IfId)
                output += indentation;
            i++;
        }
        output += ")";
        expressionDepth--;
        if (expressionDepth == 0)
            output += ";\n";
        return;
    }
    case wasm::Expression::BlockId:
    {
        wasm::Block *block = static_cast<wasm::Block *>(expression);
        if (block == nullptr)
            return;
        if (expressionDepth != 0)
            indentation += "    ";
        else
            output += indentation;
        if (block->name.str != nullptr)
            output += std::string(block->name.str) + ":\n" + indentation;
        output += "{\n";
        indentation += "    ";
        size_t _expressionDepth = expressionDepth;
        expressionDepth = 0;
        for (wasm::Expression *expression : block->list)
            GetWasm2cExperssion(output, expression, depth + 1);
        expressionDepth = _expressionDepth;
        indentation = indentation.substr(4);
        output += indentation + "}";
        if (expressionDepth != 0)
        {
            indentation = indentation.substr(4);
            output += "\n" + indentation;
        }
        else
            output += "\n";
        return;
    }
    case wasm::Expression::LocalGetId:
    {
        wasm::LocalGet *localGetExpression = static_cast<wasm::LocalGet *>(expression);

        if (expressionDepth != 0)
            output += "v" + std::to_string(localGetExpression->index);
        else
            output += indentation + "return v" + std::to_string(localGetExpression->index) + ";\n";
        return;
    }
    case wasm::Expression::LoadId:
    {
        wasm::Load *loadInstruction = static_cast<wasm::Load *>(expression);

        if (expressionDepth == 0)
            output += indentation + "return ";

        if (loadInstruction->bytes == 1)
        {
            output += "u8[";
            expressionDepth++;
            GetWasm2cExperssion(output, loadInstruction->ptr, depth + 1);
            expressionDepth--;
            output += " + " + std::to_string(loadInstruction->offset) + "]";
        }
        else if (loadInstruction->bytes == 2)
        {

            output += "u16[(";
            expressionDepth++;
            GetWasm2cExperssion(output, loadInstruction->ptr, depth + 1);
            expressionDepth--;
            output += " + " + std::to_string(loadInstruction->offset) + ") >> 1]";
        }
        else if (loadInstruction->bytes == 4)
        {

            output += "u32[(";
            expressionDepth++;
            GetWasm2cExperssion(output, loadInstruction->ptr, depth + 1);
            expressionDepth--;
            output += " + " + std::to_string(loadInstruction->offset) + ") >> 2]";
        }
        else if (loadInstruction->bytes == 8)
        {
            output += "u64[(";
            expressionDepth++;
            GetWasm2cExperssion(output, loadInstruction->ptr, depth + 1);
            expressionDepth--;
            output += " + " + std::to_string(loadInstruction->offset) + ") >> 3]";
        }
        else
        {
            std::cout << "load with " << std::to_string(loadInstruction->bytes) << " not supported" << std::endl;
            output += "unimplementedload" + std::to_string(loadInstruction->bytes);
        }
        if (expressionDepth == 0)
            output += ";\n";
        return;
    }
    case wasm::Expression::LocalSetId:
    {
        wasm::LocalSet *setInstruction = static_cast<wasm::LocalSet *>(expression);

        if (expressionDepth == 0)
            output += indentation;
        output += "v" + std::to_string(setInstruction->index) + " = ";
        if (setInstruction->value->_id == wasm::Expression::IfId)
        {
            indentation += "    ";
            output += "(";
        }
        expressionDepth++;
        GetWasm2cExperssion(output, setInstruction->value, depth + 1);
        expressionDepth--;

        if (setInstruction->value->_id == wasm::Expression::IfId)
        {
            indentation = indentation.substr(4);
            output += indentation + ")";
        }
        if (expressionDepth == 0)
        {
            if (setInstruction->value->_id != wasm::Expression::BlockId)
                output += ';';
            output += '\n';
        }
        return;
    }
    case wasm::Expression::BreakId:
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
        return;
    }
    case wasm::Expression::UnaryId:
    {
        wasm::Unary *unaryInstruction = static_cast<wasm::Unary *>(expression);
#define APPEND_TO_OUTPUT(x)                                              \
    case wasm::x:                                                        \
        output += std::string("__" #x) + "(";                            \
        expressionDepth++;                                               \
        GetWasm2cExperssion(output, unaryInstruction->value, depth + 1); \
        expressionDepth--;                                               \
        output += ")";                                                   \
        break;

        switch (unaryInstruction->op)
        {
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

#undef APPEND_TO_OUTPUT
        return;
    }
    case wasm::Expression::UnreachableId:
    {
        if (expressionDepth == 0)
            output += indentation;
        output += "assert(false)";
        if (expressionDepth == 0)
            output += ";\n";
        return;
    }
    case wasm::Expression::BinaryId:
    {
        wasm::Binary *instruction = static_cast<wasm::Binary *>(expression);

        if (expressionDepth == 0)
            output += indentation + "return ";

        bool grouped = instruction->left->_id == wasm::Expression::BinaryId || instruction->left->_id == wasm::Expression::UnaryId || instruction->left->_id == wasm::Expression::LocalSetId;
        if (grouped)
            output += "(";
        
        if (instruction->left->_id == wasm::Expression::IfId)
        {
            indentation += "    ";
            output += "(";
        }
        expressionDepth++;
        GetWasm2cExperssion(output, instruction->left, depth + 1);
        expressionDepth--;
        if (instruction->left->_id == wasm::Expression::IfId)
        {
            indentation = indentation.substr(4);
            output += indentation + ")";
        }
        if (grouped)
            output += ")";

        switch (instruction->op)
        {
        case wasm::AddInt32:
        case wasm::AddInt64:
        case wasm::AddFloat32:
        case wasm::AddFloat64:
            output += " + ";
            break;
        case wasm::SubInt32:
        case wasm::SubInt64:
        case wasm::SubFloat32:
        case wasm::SubFloat64:
            output += " - ";
            break;
        case wasm::MulInt32:
        case wasm::MulInt64:
        case wasm::MulFloat32:
        case wasm::MulFloat64:
            output += " * ";
            break;
        case wasm::DivSInt32:
        case wasm::DivUInt32:
        case wasm::DivSInt64:
        case wasm::DivUInt64:
        case wasm::DivFloat32:
        case wasm::DivFloat64:
            output += " / ";
            break;
        case wasm::RemSInt32:
        case wasm::RemUInt32:
        case wasm::RemSInt64:
        case wasm::RemUInt64:
            output += " % ";
            break;
        case wasm::AndInt32:
        case wasm::AndInt64:
            output += " & ";
            break;
        case wasm::OrInt32:
        case wasm::OrInt64:
            output += " | ";
            break;
        case wasm::XorInt32:
        case wasm::XorInt64:
            output += " ^ ";
            break;
        case wasm::ShlInt32:
        case wasm::ShlInt64:
            output += " << ";
            break;
        case wasm::ShrSInt32:
        case wasm::ShrUInt32:
        case wasm::ShrSInt64:
        case wasm::ShrUInt64:
            output += " >> ";
            break;
        case wasm::RotLInt32:
        case wasm::RotLInt64:
            output += " <<< ";
            break;
        case wasm::RotRInt32:
        case wasm::RotRInt64:
            output += " >>> ";
            break;
        case wasm::EqInt32:
        case wasm::EqInt64:
        case wasm::EqFloat32:
        case wasm::EqFloat64:
        case wasm::EqVecI8x16:
            output += " == ";
            break;
        case wasm::NeInt32:
        case wasm::NeInt64:
        case wasm::NeFloat32:
        case wasm::NeFloat64:
        case wasm::NeVecI8x16:
            output += " != ";
            break;
        case wasm::LtSInt32:
        case wasm::LtUInt32:
        case wasm::LtSInt64:
        case wasm::LtUInt64:
        case wasm::LtFloat32:
        case wasm::LtFloat64:
        case wasm::LtSVecI8x16:
        case wasm::LtUVecI8x16:
            output += " < ";
            break;
        case wasm::LeSInt32:
        case wasm::LeUInt32:
        case wasm::LeSInt64:
        case wasm::LeUInt64:
        case wasm::LeFloat32:
        case wasm::LeFloat64:
        case wasm::LeSVecI8x16:
        case wasm::LeUVecI8x16:
            output += " <= ";
            break;
        case wasm::GtSInt32:
        case wasm::GtUInt32:
        case wasm::GtSInt64:
        case wasm::GtUInt64:
        case wasm::GtFloat32:
        case wasm::GtFloat64:
        case wasm::GtSVecI8x16:
        case wasm::GtUVecI8x16:
            output += " > ";
            break;
        case wasm::GeSInt32:
        case wasm::GeUInt32:
        case wasm::GeSInt64:
        case wasm::GeUInt64:
        case wasm::GeFloat32:
        case wasm::GeFloat64:
        case wasm::GeSVecI8x16:
        case wasm::GeUVecI8x16:
            output += " >= ";
            break;
        case wasm::MinFloat32:
        case wasm::MinFloat64:
            output += " min ";
            break;
        case wasm::MaxFloat32:
        case wasm::MaxFloat64:
            output += " max";
            break;
        case wasm::CopySignFloat32:
        case wasm::CopySignFloat64:
            output += " CopySign ";
            break;
        default:
            std::cout << "could not determine binary operator for #" << std::to_string(instruction->op) << std::endl;
            output += " #" + std::to_string(instruction->op) + " ";
        }

        grouped = instruction->right->_id == wasm::Expression::BinaryId || instruction->right->_id == wasm::Expression::UnaryId || instruction->right->_id == wasm::Expression::LocalSetId;
        if (grouped)
            output += "(";
        if (instruction->right->_id == wasm::Expression::IfId)
        {
            indentation += "    ";
            output += "(";
        }
        expressionDepth++;
        GetWasm2cExperssion(output, instruction->right, depth + 1);
        expressionDepth--;
        if (instruction->right->_id == wasm::Expression::IfId)
        {
            indentation = indentation.substr(4);
            output += indentation + ")";
        }
        if (grouped)
            output += ")";

        if (expressionDepth == 0)
            output += ";\n";
        return;
    }
    case wasm::Expression::StoreId:
    {
        wasm::Store *instruction = static_cast<wasm::Store *>(expression);

        if (expressionDepth == 0)
            output += indentation;

        if (instruction->bytes == 1)
        {
            output += "u8[";
            expressionDepth++;
            GetWasm2cExperssion(output, instruction->ptr, depth + 1);
            expressionDepth--;
            output += " + " + std::to_string(instruction->offset) + "]";
        }
        else if (instruction->bytes == 2)
        {

            output += "u16[(";
            expressionDepth++;
            GetWasm2cExperssion(output, instruction->ptr, depth + 1);
            expressionDepth--;
            output += " + " + std::to_string(instruction->offset) + ") >> 1]";
        }
        else if (instruction->bytes == 4)
        {

            output += "u32[(";
            expressionDepth++;
            GetWasm2cExperssion(output, instruction->ptr, depth + 1);
            expressionDepth--;
            output += " + " + std::to_string(instruction->offset) + ") >> 2]";
        }
        else if (instruction->bytes == 8)
        {
            output += "u64[(";
            expressionDepth++;
            GetWasm2cExperssion(output, instruction->ptr, depth + 1);
            expressionDepth--;
            output += " + " + std::to_string(instruction->offset) + ") >> 3]";
        }
        else
        {
            std::cout << "store with " << std::to_string(instruction->bytes) << " not supported" << std::endl;
            output += "unimplementedstore" + std::to_string(instruction->bytes);
        }

        output += " = ";

        expressionDepth++;
        GetWasm2cExperssion(output, instruction->value, depth + 1);
        expressionDepth--;

        if (expressionDepth == 0)
        {
            if (instruction->value->_id != wasm::Expression::BlockId)
                output += ';';
            output += '\n';
        }
        return;
    }
    case wasm::Expression::ConstId:
    {
        wasm::Const *instruction = static_cast<wasm::Const *>(expression);

        if (expressionDepth == 0)
            output += indentation + "return ";
        if (instruction->type == wasm::Type::f32)
            output += std::to_string(instruction->value.getf32());
        else if (instruction->type == wasm::Type::f64)
            output += std::to_string(instruction->value.getf64());
        else if (instruction->type == wasm::Type::i32)
            output += std::to_string(instruction->value.geti32());
        else if (instruction->type == wasm::Type::i64)
            output += std::to_string(instruction->value.geti64());
        else
        {
            std::cout << "unable to convert wasm const id " << std::to_string(instruction->type.getID()) << " to string" << std::endl;
            output += "const" + std::to_string(instruction->type.getID());
        }

        if (expressionDepth == 0)
            output += ";\n";
        return;
    }
    case wasm::Expression::IfId:
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
            if (output.back() != '\n')
                output += '\n';
            output += indentation + "else\n";
            if (instruction->ifFalse->_id != wasm::Expression::BlockId)
                indentation += "    ";

            _expressionDepth = expressionDepth;
            expressionDepth = 0;
            GetWasm2cExperssion(output, instruction->ifFalse, depth + 1);
            expressionDepth = _expressionDepth;
            if (instruction->ifFalse->_id != wasm::Expression::BlockId)
                indentation = indentation.substr(4);
        }
        return;
    }
    case wasm::Expression::DropId:
    {
        wasm::Drop *instruction = static_cast<wasm::Drop *>(expression);

        // if (expressionDepth == 0)
        //     output += indentation;
        // output += "drop ";
        GetWasm2cExperssion(output, instruction->value, depth + 1);
        // output += '\n';
        return;
    }
    case wasm::Expression::SwitchId:
    {
        wasm::Switch *instruction = static_cast<wasm::Switch *>(expression);

        output += indentation + "switch(";
        expressionDepth++;
        GetWasm2cExperssion(output, instruction->condition, depth + 1);
        expressionDepth--;
        output += ")\n" + indentation + "{\n";
        indentation += "    ";
        for (size_t i = 0; i < instruction->targets.size(); i++)
        {
            output += indentation + "case " + std::to_string(i) + ": break " + instruction->targets[i].str + ";\n";
        }
        indentation = indentation.substr(4);

        output += indentation + "}\n";
        return;
    }
    case wasm::Expression::ReturnId:
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
        return;
    }
    case wasm::Expression::GlobalSetId:
    {
        wasm::GlobalSet *instruction = static_cast<wasm::GlobalSet *>(expression);

        output += indentation + instruction->name.str + " = ";

        expressionDepth++;
        GetWasm2cExperssion(output, instruction->value, depth);
        expressionDepth--;

        output += ";\n";
        return;
    }
    case wasm::Expression::GlobalGetId:
    {
        wasm::GlobalGet *instruction = static_cast<wasm::GlobalGet *>(expression);

        if (expressionDepth == 0)
            output += indentation;

        output += instruction->name.str;

        if (expressionDepth == 0)
            output += ";\n";
        return;
    }
    case wasm::Expression::LoopId:
    {
        wasm::Loop *instruction = static_cast<wasm::Loop *>(expression);
        output += indentation + "while (true)\n";
        GetWasm2cExperssion(output, instruction->body, depth + 1);
        return;
    }
    case wasm::Expression::SelectId:
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
        return;
    }
    case wasm::Expression::MemorySizeId:
    {
        output += "MEMORY_SIZE";
        return;
    }
    case wasm::Expression::NopId:
    {
        return;
    }
    case wasm::Expression::CallIndirectId:
    {
        wasm::CallIndirect *instruction = static_cast<wasm::CallIndirect *>(expression);
        if (expressionDepth == 0)
            output += indentation;
        output += "FUNCTION_TABLE[";
        expressionDepth++;
        GetWasm2cExperssion(output, instruction->target, depth + 1);
        expressionDepth--;
        output += "](";
        expressionDepth++;
        size_t i = 0;
        for (wasm::Expression *operand : instruction->operands)
        {
            GetWasm2cExperssion(output, operand, depth + 1);
            if (i != instruction->operands.size() - 1)
                output += ", ";
            i++;
        }
        output += ")";
        expressionDepth--;
        if (expressionDepth == 0)
            output += ";\n";
        return;
    }
    default:
    {
        if (expressionDepth == 0)
            output += indentation;
        output += "unimplemented" + std::to_string(id);
        if (expressionDepth == 0)
            output += ";\n";
    }
    }
}
std::string GetWasm2cFunctionBody(wasm::Function *function)
{
    std::string output;

    if (function->body == nullptr)
        return "// imported\n";

    GetWasm2cExperssion(output, function->body, 0);

    return output;
}
std::string GetWasm2cFunctionLocals(wasm::Function *function)
{
    std::string output;

    for (size_t i = function->getNumParams() - 1; i < function->getNumVars(); i++)
    {
        std::string local;

        wasm::Type type = function->getLocalType(i);

        local += GetStringFromWasmType(type);

        output += indentation + local + " v" + std::to_string(i + 1) + ";\n";
    }

    return output;
}
std::string GenerateWasm2cFunctionBodies(wasm::Module *module)
{
    std::string output;
    for (std::unique_ptr<wasm::Function> &function : module->functions)
    {
        std::string body;
        body += GetFunctionSignature(function.get());

        body += "\n{\n"; // open function body
        indentation += "    ";

        body += GetWasm2cFunctionLocals(function.get());
        body += GetWasm2cFunctionBody(function.get());

        indentation = indentation.substr(4);
        body += "}";

        output += body;
        output += "\n\n";
    }

    return output;
}
std::string GenerateWasm2cFunctionDeclarations(wasm::Module *module)
{
    std::string output;
    for (std::unique_ptr<wasm::Function> &function : module->functions)
    {
        std::string declaration;

        declaration += GetFunctionSignature(function.get());

        declaration += ";\n";

        output += declaration;
    }

    return output;
}
std::string GenerateWasm2cMemory(wasm::Module *module)
{
    std::string output;
    uint64_t memorySize = module->memory.max.addr;

    output += "uint8_t *u8 = (uint8_t *)0\n"
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
std::string GenerateWasm2cGlobals(wasm::Module *module)
{
    std::string globals;
    for (std::unique_ptr<wasm::Global> &global : module->globals)
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
std::string GenerateWasm2c(wasm::Module *module)
{
    std::string output;
    output += "#include <stdint.h>\n"
              "\n";

    output += GenerateWasm2cGlobals(module);
    output += GenerateWasm2cMemory(module);
    output += GenerateWasm2cFunctionDeclarations(module);
    output += GenerateWasm2cFunctionBodies(module);

    return output;
}
void WriteOutput(wasm::Module *module, const std::string &outputFile)
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
