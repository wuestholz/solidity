#include <libsolidity/ast/AST.h>
#include <libsolidity/ast/ASTBoogieUtils.h>
#include <libsolidity/ast/BoogieContext.h>
#include <libsolidity/ast/BoogieAst.h>
#include <libsolidity/ast/TypeProvider.h>

#include <liblangutil/ErrorReporter.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;
using namespace langutil;

namespace bg = boogie;

namespace dev
{
namespace solidity
{

BoogieContext::BoogieGlobalContext::BoogieGlobalContext()
{
	// Remove all variables, so we can just add our own
	m_magicVariables.clear();

	// Add magic variables for the 'sum' function for all sizes of int and uint
	for (string sumType: { "int", "uint" })
	{
		auto funType = TypeProvider::function(strings { }, strings { sumType },
				FunctionType::Kind::Internal, true, StateMutability::Pure);
		auto sum = new MagicVariableDeclaration(ASTBoogieUtils::VERIFIER_SUM + "_" + sumType, funType);
		m_magicVariables.push_back(shared_ptr<MagicVariableDeclaration const>(sum));
	}

	// Add magic variables for the 'old' function
	for (string oldType: { "address", "bool", "int", "uint" })
	{
		auto funType = TypeProvider::function(strings { oldType }, strings { oldType },
				FunctionType::Kind::Internal, false, StateMutability::Pure);
		auto old = new MagicVariableDeclaration(ASTBoogieUtils::VERIFIER_OLD + "_" + oldType, funType);
		m_magicVariables.push_back(shared_ptr<MagicVariableDeclaration const>(old));
	}
}

BoogieContext::BoogieContext(Encoding encoding,
		bool overflow,
		bool modAnalysis,
		ErrorReporter* errorReporter,
		std::map<ASTNode const*,
		std::shared_ptr<DeclarationContainer>> scopes,
		EVMVersion evmVersion)
:
		m_program(), m_encoding(encoding), m_overflow(overflow), m_modAnalysis(modAnalysis), m_errorReporter(errorReporter),
		m_currentScanner(nullptr), m_scopes(scopes), m_evmVersion(evmVersion),
		m_currentContractInvars(), m_currentSumDecls(), m_builtinFunctions(),
		m_transferIncluded(false), m_callIncluded(false), m_sendIncluded(false)
{
	// Initialize global declarations
	addGlobalComment("Global declarations and definitions related to the address type");
	// address type
	addDecl(addressType());
	// address.balance
	m_boogieBalance = bg::Decl::variable("__balance", ASTBoogieUtils::mappingType(addressType(), intType(256)));
	addDecl(m_boogieBalance);
	// This, sender, value
	m_boogieThis = bg::Decl::variable("__this", addressType());
	m_boogieMsgSender = bg::Decl::variable("__msg_sender", addressType());
	m_boogieMsgValue = bg::Decl::variable("__msg_value", intType(256));
	// Uninterpreted type for strings
	addDecl(stringType());
	// now
	addDecl(bg::Decl::variable(ASTBoogieUtils::BOOGIE_NOW, intType(256)));
	// block number
	addDecl(bg::Decl::variable(ASTBoogieUtils::BOOGIE_BLOCKNO, intType(256)));
	// overflow
	if (m_overflow)
		addDecl(bg::Decl::variable(ASTBoogieUtils::VERIFIER_OVERFLOW, boolType()));
}

string BoogieContext::mapDeclName(Declaration const& decl)
{
	// Check for special names
	if (dynamic_cast<MagicVariableDeclaration const*>(&decl))
	{
		if (decl.name() == ASTBoogieUtils::SOLIDITY_ASSERT) return decl.name();
		if (decl.name() == ASTBoogieUtils::SOLIDITY_REQUIRE) return decl.name();
		if (decl.name() == ASTBoogieUtils::SOLIDITY_REVERT) return decl.name();
		if (decl.name() == ASTBoogieUtils::SOLIDITY_THIS) return m_boogieThis->getName();
		if (decl.name() == ASTBoogieUtils::SOLIDITY_NOW) return ASTBoogieUtils::BOOGIE_NOW;
	}

	// ID is important to append, since (1) even fully qualified names can be
	// same for state variables and local variables in functions, (2) return
	// variables might have no name (whereas Boogie requires a name)
	string name = decl.name() + "#" + to_string(decl.id());

	// Check if the current declaration is enclosed by any of the
	// extra scopes, if yes, add extra ID
	for (auto extraScope: m_extraScopes)
	{
		ASTNode const* running = decl.scope();
		while (running)
		{
			if (running == extraScope.first)
			{
				name += "#" + extraScope.second;
				break;
			}
			running = m_scopes[running]->enclosingNode();
		}
	}

	return name;
}

void BoogieContext::addBuiltinFunction(bg::FuncDeclRef fnDecl)
{
	m_builtinFunctions[fnDecl->getName()] = fnDecl;
	m_program.getDeclarations().push_back(fnDecl);
}

void BoogieContext::includeTransferFunction()
{
	if (m_transferIncluded) return;
	m_transferIncluded = true;
	m_program.getDeclarations().push_back(ASTBoogieUtils::createTransferProc(*this));
}

void BoogieContext::includeCallFunction()
{
	if (m_callIncluded) return;
	m_callIncluded = true;
	m_program.getDeclarations().push_back(ASTBoogieUtils::createCallProc(*this));
}

void BoogieContext::includeSendFunction()
{
	if (m_sendIncluded) return;
	m_sendIncluded = true;
	m_program.getDeclarations().push_back(ASTBoogieUtils::createSendProc(*this));
}

void BoogieContext::reportError(ASTNode const* associatedNode, string message)
{
	solAssert(associatedNode, "Error at unknown node: " + message);
	m_errorReporter->error(Error::Type::ParserError, associatedNode->location(), message);
}

void BoogieContext::reportWarning(ASTNode const* associatedNode, string message)
{
	solAssert(associatedNode, "Warning at unknown node: " + message);
	m_errorReporter->warning(associatedNode->location(), message);
}

void BoogieContext::addGlobalComment(string str)
{
	addDecl(bg::Decl::comment("", str));
}

void BoogieContext::addDecl(bg::Decl::Ref decl)
{
	m_program.getDeclarations().push_back(decl);
}

void BoogieContext::addConstant(bg::Decl::Ref decl)
{
	bool alreadyDefined = false;
	for (auto d: m_constants)
	{
		if (d->getName() == decl->getName())
		{
			// TODO: check that other fields are equal
			alreadyDefined = true;
			break;
		}
	}
	if (!alreadyDefined)
	{
		addDecl(decl);
		m_constants.push_back(decl);
	}
}

bg::TypeDeclRef BoogieContext::addressType() const
{
	bg::TypeDeclRef it = intType(256);
	return bg::Decl::typee("address_t", it->getName());
}

bg::TypeDeclRef BoogieContext::boolType() const
{
	return bg::Decl::typee("bool");
}

bg::TypeDeclRef BoogieContext::stringType() const
{
	return bg::Decl::typee("string_t");
}

bg::TypeDeclRef BoogieContext::intType(unsigned size) const
{
	if (isBvEncoding())
		return bg::Decl::typee("bv" + toString(size));
	else
		return bg::Decl::typee("int");
}

bg::FuncDeclRef BoogieContext::getStructConstructor(StructDefinition const* structDef)
{
	if (m_storStructConstrs.find(structDef) == m_storStructConstrs.end())
	{
		vector<bg::Binding> params;

		for (auto member: structDef->members())
		{
			// Make sure that the location of the member is storage (this is
			// important for struct members as there is a single type per struct
			// definition, which is storage pointer by default).
			// TODO: can we do better?
			TypePointer memberType = TypeProvider::withLocationIfReference(DataLocation::Storage, member->type());
			params.push_back({
				bg::Expr::id(mapDeclName(*member)),
				toBoogieType(memberType, structDef)});
		}

		vector<bg::Attr::Ref> attrs;
		attrs.push_back(bg::Attr::attr("constructor"));
		string name = structDef->name() + "#" + toString(structDef->id()) + "#constr";
		m_storStructConstrs[structDef] = bg::Decl::function(name, params,
				getStructType(structDef, DataLocation::Storage), nullptr, attrs);
		addDecl(m_storStructConstrs[structDef]);
	}
	return m_storStructConstrs[structDef];
}

bg::TypeDeclRef BoogieContext::getStructType(StructDefinition const* structDef, DataLocation loc)
{
	string typeName = "struct_" + ASTBoogieUtils::dataLocToStr(loc) +
			"_" + structDef->name() + "#" + toString(structDef->id());

	if (loc == DataLocation::Storage)
	{
		if (m_storStructTypes.find(structDef) == m_storStructTypes.end())
		{
			vector<bg::Binding> members;
			for (auto member: structDef->members())
			{
				// Make sure that the location of the member is storage (this is
				// important for struct members as there is a single type per struct
				// definition, which is storage pointer by default).
				// TODO: can we do better?
				TypePointer memberType = TypeProvider::withLocationIfReference(loc, member->type());
				members.push_back({bg::Expr::id(mapDeclName(*member)),
					toBoogieType(memberType, structDef)});
			}
			m_storStructTypes[structDef] = bg::Decl::datatype(typeName, members);
			addDecl(m_storStructTypes[structDef]);
			getStructConstructor(structDef);
		}
		return m_storStructTypes[structDef];
	}
	if (loc == DataLocation::Memory)
	{
		if (m_memStructTypes.find(structDef) == m_memStructTypes.end())
		{
			m_memStructTypes[structDef] = bg::Decl::typee("address_" + typeName);
			addDecl(m_memStructTypes[structDef]);
		}
		return m_memStructTypes[structDef];
	}

	solAssert(false, "Unsupported data location for structs");
	return nullptr;
}

bg::TypeDeclRef BoogieContext::toBoogieType(TypePointer tp, ASTNode const* _associatedNode)
{
	Type::Category tpCategory = tp->category();

	switch (tpCategory)
	{
	case Type::Category::Address:
		return addressType();
	case Type::Category::StringLiteral:
		return stringType();
	case Type::Category::Bool:
		return boolType();
	case Type::Category::RationalNumber:
	{
		auto tpRational = dynamic_cast<RationalNumberType const*>(tp);
		if (!tpRational->isFractional())
			return bg::Decl::typee(ASTBoogieUtils::BOOGIE_INT_CONST_TYPE);
		else
			reportError(_associatedNode, "Fractional numbers are not supported");
		break;
	}
	case Type::Category::Integer:
	{
		auto tpInteger = dynamic_cast<IntegerType const*>(tp);
		return intType(tpInteger->numBits());
	}
	case Type::Category::Contract:
		return addressType();
	case Type::Category::Array:
	{
		auto arrType = dynamic_cast<ArrayType const*>(tp);
		if (arrType->isString())
			return stringType();

		Type const* baseType = arrType->baseType();
		bg::TypeDeclRef baseTypeBoogie = toBoogieType(baseType, _associatedNode);

		// Declare datatype and constructor if needed
		if (m_arrDataTypes.find(baseTypeBoogie->getName()) == m_arrDataTypes.end())
		{
			// Datatype: [int]T + length
			vector<bg::Binding> members = {
					{bg::Expr::id("arr"), ASTBoogieUtils::mappingType(intType(256), baseTypeBoogie)},
				{bg::Expr::id("length"), intType(256)}};
			m_arrDataTypes[baseTypeBoogie->getName()] = bg::Decl::datatype(baseTypeBoogie->getName() + "_arr_type", members);
			addDecl(m_arrDataTypes[baseTypeBoogie->getName()]);

			// Constructor for datatype
			vector<bg::Attr::Ref> attrs;
			attrs.push_back(bg::Attr::attr("constructor"));
			string name = baseTypeBoogie->getName() + "_arr#constr";
			m_arrConstrs[baseTypeBoogie->getName()] = bg::Decl::function(name, members,
					m_arrDataTypes[baseTypeBoogie->getName()], nullptr, attrs);
			addDecl(m_arrConstrs[baseTypeBoogie->getName()]);
		}

		// Storage arrays are simply the data structures
		if (arrType->location() == DataLocation::Storage)
			return m_arrDataTypes[baseTypeBoogie->getName()];
		// Memory arrays have an extra layer of indirection
		else if (arrType->location() == DataLocation::Memory)
		{
			if (m_memArrPtrTypes.find(baseTypeBoogie->getName()) == m_memArrPtrTypes.end())
			{
				// Pointer type
				m_memArrPtrTypes[baseTypeBoogie->getName()] = bg::Decl::typee(baseTypeBoogie->getName() + "_arr_ptr");
				addDecl(m_memArrPtrTypes[baseTypeBoogie->getName()]);

				// The actual storage
				m_memArrs[baseTypeBoogie->getName()] = bg::Decl::variable("mem_arr_" + baseTypeBoogie->getName(),
						ASTBoogieUtils::mappingType(
								m_memArrPtrTypes[baseTypeBoogie->getName()],
								m_arrDataTypes[baseTypeBoogie->getName()]));
				addDecl(m_memArrs[baseTypeBoogie->getName()]);
			}
			return m_memArrPtrTypes[baseTypeBoogie->getName()];
		}
		else
		{
			reportError(_associatedNode, "Unsupported location for array type");
			return bg::Decl::typee(ASTBoogieUtils::ERR_TYPE);
		}
		break;
	}
	case Type::Category::Mapping:
	{
		auto mapType = dynamic_cast<MappingType const*>(tp);
		return ASTBoogieUtils::mappingType(toBoogieType(mapType->keyType(), _associatedNode),
				toBoogieType(mapType->valueType(), _associatedNode));
	}
	case Type::Category::FixedBytes:
	{
		// up to 32 bytes (use integer and slice it up)
		auto fbType = dynamic_cast<FixedBytesType const*>(tp);
		return intType(fbType->numBytes() * 8);
	}
	case Type::Category::Tuple:
		reportError(_associatedNode, "Tuples are not supported");
		break;
	case Type::Category::Struct:
	{
		auto structTp = dynamic_cast<StructType const*>(tp);
		if (structTp->location() == DataLocation::Storage && structTp->isPointer())
			reportError(_associatedNode, "Local storage pointers are not supported");
		return getStructType(&structTp->structDefinition(), structTp->location());
	}
	case Type::Category::Enum:
		return intType(256);
	default:
		std::string tpStr = tp->toString();
		reportError(_associatedNode, "Unsupported type: '" + tpStr.substr(0, tpStr.find(' ')) + "'");
	}

	return bg::Decl::typee(ASTBoogieUtils::ERR_TYPE);
}

bg::Expr::Ref BoogieContext::intLit(bg::bigint lit, int bits) const
{
	if (isBvEncoding())
		return bg::Expr::lit(lit, bits);
	else
		return bg::Expr::lit(lit);
}

bg::Expr::Ref BoogieContext::intSlice(bg::Expr::Ref base, unsigned size, unsigned high, unsigned low)
{
	solAssert(high < size, "");
	solAssert(low < high, "");
	if (isBvEncoding())
		return bvExtract(base, size, high, low);
	else
	{
		bg::Expr::Ref result = base;
		if (low > 0)
		{
			bg::Expr::Ref c1 = bg::Expr::lit(bg::bigint(2) << (low - 1));
			result = bg::Expr::intdiv(result, c1);
		}
		if (high < size - 1)
		{
			bg::Expr::Ref c2 = bg::Expr::lit(bg::bigint(2) << (high - low));
			result = bg::Expr::mod(result, c2);
		}
		return result;
	}
}

bg::Expr::Ref BoogieContext::bvExtract(bg::Expr::Ref expr, unsigned exprSize, unsigned high, unsigned low)
{
	// Function name
	std::stringstream fnNameSS;
	fnNameSS << "extract_" << high << "_to_" << low << "_from_" << exprSize;
	std::string fnName = fnNameSS.str();

	// Get it if already there
	if (m_builtinFunctions.find(fnName) == m_builtinFunctions.end())
	{
		// Not there construct SMT
		std::stringstream fnSmtSS;
		fnSmtSS << "(_ extract " << high << " " << low << "0)";

		// Appropriate types
		unsigned resultSize = high - low + 1;
		bg::TypeDeclRef resultType = intType(resultSize);
		bg::TypeDeclRef exprType = intType(exprSize);

		// Boogie declaration
		bg::FuncDeclRef fnDecl = bg::Decl::function(
				fnNameSS.str(),     // Name
				{ { bg::Expr::id(""), exprType} }, // Arguments
				resultType,         // Type
				nullptr,            // Body = null
				{ bg::Attr::attr("bvbuiltin", fnSmtSS.str()) } // Attributes
		);

		// Add it
		addBuiltinFunction(fnDecl);
	}

	return bg::Expr::fn(fnName, expr);
}

bg::Expr::Ref BoogieContext::bvZeroExt(bg::Expr::Ref expr, unsigned exprSize, unsigned resultSize)
{
	// Function name
	std::stringstream fnNameSS;
	fnNameSS << "bvzeroext_" << exprSize << "_to_" << resultSize;
	std::string fnName = fnNameSS.str();

	// Get it if already there
	if (m_builtinFunctions.find(fnName) == m_builtinFunctions.end())
	{
		// Not there construct SMT
		std::stringstream fnSmtSS;
		fnSmtSS << "(_ zero_extend " << resultSize - exprSize << ")";

		// Appropriate types
		bg::TypeDeclRef resultType = intType(resultSize);
		bg::TypeDeclRef exprType = intType(exprSize);

		// Boogie declaration
		bg::FuncDeclRef fnDecl = bg::Decl::function(
				fnNameSS.str(),     // Name
				{ { bg::Expr::id(""), exprType} }, // Arguments
				resultType,         // Type
				nullptr,            // Body = null
				{ bg::Attr::attr("bvbuiltin", fnSmtSS.str()) } // Attributes
		);

		// Add it
		addBuiltinFunction(fnDecl);
	}

	return bg::Expr::fn(fnName, expr);
}

bg::Expr::Ref BoogieContext::bvSignExt(bg::Expr::Ref expr, unsigned exprSize, unsigned resultSize)
{
	// Function name
	std::stringstream fnNameSS;
	fnNameSS << "bvsignext_" << exprSize << "_to_" << resultSize;
	std::string fnName = fnNameSS.str();

	// Get it if already there
	if (m_builtinFunctions.find(fnName) == m_builtinFunctions.end())
	{
		// Not there construct SMT
		std::stringstream fnSmtSS;
		fnSmtSS << "(_ sign_extend " << resultSize - exprSize << ")";

		// Appropriate types
		bg::TypeDeclRef resultType = intType(resultSize);
		bg::TypeDeclRef exprType = intType(exprSize);

		// Boogie declaration
		bg::FuncDeclRef fnDecl = bg::Decl::function(
				fnNameSS.str(),     // Name
				{ { bg::Expr::id(""), exprType} }, // Arguments
				resultType,         // Type
				nullptr,            // Body = null
				{ bg::Attr::attr("bvbuiltin", fnSmtSS.str()) } // Attributes
		);

		// Add it
		addBuiltinFunction(fnDecl);
	}

	return bg::Expr::fn(fnName, expr);
}

bg::Expr::Ref BoogieContext::bvNeg(unsigned bits, bg::Expr::Ref expr)
{
	return bvUnaryOp("neg", bits, expr);
}

bg::Expr::Ref BoogieContext::bvNot(unsigned bits, bg::Expr::Ref expr)
{
	return bvUnaryOp("not", bits, expr);
}


bg::Expr::Ref BoogieContext::bvAdd(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("add", bits, lhs, rhs);
}

bg::Expr::Ref BoogieContext::bvSub(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("sub", bits, lhs, rhs);
}

bg::Expr::Ref BoogieContext::bvMul(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("mul", bits, lhs, rhs);
}

bg::Expr::Ref BoogieContext::bvSDiv(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("sdiv", bits, lhs, rhs);
}

bg::Expr::Ref BoogieContext::bvUDiv(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("udiv", bits, lhs, rhs);
}

bg::Expr::Ref BoogieContext::bvAnd(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("and", bits, lhs, rhs);
}

bg::Expr::Ref BoogieContext::bvOr(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("or", bits, lhs, rhs);
}

bg::Expr::Ref BoogieContext::bvXor(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("xor", bits, lhs, rhs);
}

bg::Expr::Ref BoogieContext::bvAShr(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("ashr", bits, lhs, rhs);
}

bg::Expr::Ref BoogieContext::bvLShr(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("lshr", bits, lhs, rhs);
}

bg::Expr::Ref BoogieContext::bvShl(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("shl", bits, lhs, rhs);
}

bg::Expr::Ref BoogieContext::bvSlt(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("slt", bits, lhs, rhs, boolType());
}

bg::Expr::Ref BoogieContext::bvUlt(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("ult", bits, lhs, rhs, boolType());
}

bg::Expr::Ref BoogieContext::bvSgt(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("sgt", bits, lhs, rhs, boolType());
}

bg::Expr::Ref BoogieContext::bvUgt(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("ugt", bits, lhs, rhs, boolType());
}

bg::Expr::Ref BoogieContext::bvSle(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("sle", bits, lhs, rhs, boolType());
}

bg::Expr::Ref BoogieContext::bvUle(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("ule", bits, lhs, rhs, boolType());
}

bg::Expr::Ref BoogieContext::bvSge(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("sge", bits, lhs, rhs, boolType());
}

bg::Expr::Ref BoogieContext::bvUge(unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	return bvBinaryOp("uge", bits, lhs, rhs, boolType());
}

bg::Expr::Ref BoogieContext::bvBinaryOp(std::string name, unsigned bits, bg::Expr::Ref lhs, bg::Expr::Ref rhs, bg::TypeDeclRef resultType)
{
	// Function name
	std::stringstream fnNameSS;
	fnNameSS << "bv" << bits << name;
	std::string fnName = fnNameSS.str();

	// Get it if already there
	if (m_builtinFunctions.find(fnName) == m_builtinFunctions.end())
	{
		// Not there construct SMT
		std::stringstream fnSmtSS;
		fnSmtSS << "bv" << name;

		// Appropriate types
		if (resultType == nullptr)
			resultType = intType(bits);
		bg::TypeDeclRef exprType = intType(bits);

		// Boogie declaration
		bg::FuncDeclRef fnDecl = bg::Decl::function(
				fnNameSS.str(),     // Name
				{ { bg::Expr::id(""), exprType }, { bg::Expr::id(""), exprType } }, // Arguments
				resultType,         // Type
				nullptr,            // Body = null
				{ bg::Attr::attr("bvbuiltin", fnSmtSS.str()) } // Attributes
		);

		// Add it
		addBuiltinFunction(fnDecl);
	}

	return bg::Expr::fn(fnName, lhs, rhs);
}

bg::Expr::Ref BoogieContext::bvUnaryOp(std::string name, unsigned bits, bg::Expr::Ref expr)
{
	// Function name
	std::stringstream fnNameSS;
	fnNameSS << "bv" << bits << name;
	std::string fnName = fnNameSS.str();

	// Get it if already there
	if (m_builtinFunctions.find(fnName) == m_builtinFunctions.end())
	{
		// Not there construct SMT
		std::stringstream fnSmtSS;
		fnSmtSS << "bv" << name;

		// Appropriate types
		bg::TypeDeclRef exprType = intType(bits);

		// Boogie declaration
		bg::FuncDeclRef fnDecl = bg::Decl::function(
				fnNameSS.str(),       // Name
				{ { bg::Expr::id(""), exprType } }, // Arguments
				exprType,  // Type
				nullptr,              // Body = null
				{ bg::Attr::attr("bvbuiltin", fnSmtSS.str()) } // Attributes
		);

		// Add it
		addBuiltinFunction(fnDecl);
	}

	return bg::Expr::fn(fnName, expr);
}

}
}
