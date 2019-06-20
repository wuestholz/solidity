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

namespace dev
{
namespace solidity
{

BoogieContext::BoogieGlobalContext::BoogieGlobalContext()
{
	// Remove all variables, so we can just add our own
	m_magicVariables.clear();

	// Add magic variables for the 'sum' function for all sizes of int and uint
	for (string sumType : { "int", "uint" })
	{
		auto funType = TypeProvider::function(strings { }, strings { sumType },
				FunctionType::Kind::Internal, true, StateMutability::Pure);
		auto sum = new MagicVariableDeclaration(ASTBoogieUtils::VERIFIER_SUM + "_" + sumType, funType);
		m_magicVariables.push_back(shared_ptr<MagicVariableDeclaration const>(sum));
	}

	// Add magic variables for the 'old' function
	for (string oldType : { "address", "bool", "int", "uint" })
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
	m_boogieBalance = boogie::Decl::variable("__balance", ASTBoogieUtils::mappingType(addressType(), intType(256)));
	addDecl(m_boogieBalance);
	// This
	m_boogieThis = boogie::Decl::variable("__this", addressType());
	// Uninterpreted type for strings
	addDecl(stringType());
	// now
	addDecl(boogie::Decl::variable(ASTBoogieUtils::BOOGIE_NOW, intType(256)));
	// block number
	addDecl(boogie::Decl::variable(ASTBoogieUtils::BOOGIE_BLOCKNO, intType(256)));
	// overflow
	if (m_overflow)
		addDecl(boogie::Decl::variable(ASTBoogieUtils::VERIFIER_OVERFLOW, boolType()));
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
	for (auto extraScope : m_extraScopes)
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

string BoogieContext::mapStructMemberName(Declaration const& decl, DataLocation loc)
{
	return mapDeclName(decl) + "_" + ASTBoogieUtils::dataLocToStr(loc);
}

void BoogieContext::addBuiltinFunction(boogie::FuncDeclRef fnDecl)
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
	if (associatedNode)
		m_errorReporter->error(Error::Type::ParserError, associatedNode->location(), message);
	else
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Error at unknown node: " + message));
}

void BoogieContext::reportWarning(ASTNode const* associatedNode, string message)
{
	if (associatedNode)
		m_errorReporter->warning(associatedNode->location(), message);
	else
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Warning at unknown node: " + message));
}

void BoogieContext::addGlobalComment(string str)
{
	addDecl(boogie::Decl::comment("", str));
}

void BoogieContext::addDecl(boogie::Decl::Ref decl)
{
	m_program.getDeclarations().push_back(decl);
}

void BoogieContext::addConstant(boogie::Decl::Ref decl)
{
	bool alreadyDefined = false;
	for (auto d : m_constants)
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

boogie::TypeDeclRef BoogieContext::addressType() const
{
	boogie::TypeDeclRef it = intType(256);
	return boogie::Decl::typee("address_t", it->getName());
}

boogie::TypeDeclRef BoogieContext::boolType() const
{
	return boogie::Decl::typee("bool");
}

boogie::TypeDeclRef BoogieContext::stringType() const
{
	return boogie::Decl::typee("string_t");
}

boogie::TypeDeclRef BoogieContext::intType(unsigned size) const
{
	if (isBvEncoding())
		return boogie::Decl::typee("bv" + toString(size));
	else
		return boogie::Decl::typee("int");
}

boogie::FuncDeclRef BoogieContext::createStructConstructor(StructDefinition const* structDef)
{
	if (m_storStructConstrs.find(structDef) == m_storStructConstrs.end())
	{
		vector<boogie::Binding> params;

		for (auto member : structDef->members())
		{
			// Make sure that the location of the member is storage (this is
			// important for struct members as there is a single type per struct
			// definition, which is storage pointer by default).
			// TODO: can we do better?
			TypePointer memberType = TypeProvider::withLocationIfReference(DataLocation::Storage, member->type());
			params.push_back({
				boogie::Expr::id(mapDeclName(*member)),
				ASTBoogieUtils::toBoogieType(memberType, structDef, *this)});
		}

		vector<boogie::Attr::Ref> attrs;
		attrs.push_back(boogie::Attr::attr("constructor"));
		string name = structDef->name() + "#" + toString(structDef->id()) + "#constr";
		m_storStructConstrs[structDef] = boogie::Decl::function(name, params,
				getStructType(structDef, DataLocation::Storage), nullptr, attrs);
		addDecl(m_storStructConstrs[structDef]);
	}
	return m_storStructConstrs[structDef];
}

boogie::TypeDeclRef BoogieContext::getStructType(StructDefinition const* structDef, DataLocation loc)
{
	string typeName = "struct_" + ASTBoogieUtils::dataLocToStr(loc) +
			"_" + structDef->name() + "#" + toString(structDef->id());

	if (loc == DataLocation::Storage)
	{
		if (m_storStructTypes.find(structDef) == m_storStructTypes.end())
		{
			vector<boogie::Binding> members;
			for (auto member : structDef->members())
			{
				// Make sure that the location of the member is storage (this is
				// important for struct members as there is a single type per struct
				// definition, which is storage pointer by default).
				// TODO: can we do better?
				TypePointer memberType = TypeProvider::withLocationIfReference(loc, member->type());
				members.push_back({boogie::Expr::id(mapDeclName(*member)),
					ASTBoogieUtils::toBoogieType(memberType, structDef, *this)});
			}
			m_storStructTypes[structDef] = boogie::Decl::datatype(typeName, members);
			addDecl(m_storStructTypes[structDef]);
			createStructConstructor(structDef);
		}
		return m_storStructTypes[structDef];
	}
	if (loc == DataLocation::Memory)
	{
		if (m_memStructTypes.find(structDef) == m_memStructTypes.end())
		{
			m_memStructTypes[structDef] = boogie::Decl::typee("address_" + typeName);
			addDecl(m_memStructTypes[structDef]);
		}
		return m_memStructTypes[structDef];
	}

	solAssert(false, "Unsupported data location for structs");
	return nullptr;
}

boogie::Expr::Ref BoogieContext::boogieBalance() const
{
	return boogie::Expr::id(m_boogieBalance->getName());
}

boogie::Expr::Ref BoogieContext::boogieThis() const
{
	return boogie::Expr::id(m_boogieThis->getName());
}

boogie::Expr::Ref BoogieContext::boogieMsgSender() const
{
	return boogie::Expr::id("__msg_sender");
}

boogie::Expr::Ref BoogieContext::boogieMsgValue() const
{
	return boogie::Expr::id("__msg_value");
}

boogie::Expr::Ref BoogieContext::intLit(boogie::bigint lit, int bits) const
{
	if (isBvEncoding())
		return boogie::Expr::lit(lit, bits);
	else
		return boogie::Expr::lit(lit);
}

boogie::Expr::Ref BoogieContext::intSlice(boogie::Expr::Ref base, unsigned size, unsigned high, unsigned low)
{
	solAssert(high < size, "");
	solAssert(low < high, "");
	if (isBvEncoding())
		return bvExtract(base, size, high, low);
	else
	{
		boogie::Expr::Ref result = base;
		if (low > 0)
		{
			boogie::Expr::Ref c1 = boogie::Expr::lit(boogie::bigint(2) << (low - 1));
			result = boogie::Expr::intdiv(result, c1);
		}
		if (high < size - 1)
		{
			boogie::Expr::Ref c2 = boogie::Expr::lit(boogie::bigint(2) << (high - low));
			result = boogie::Expr::mod(result, c2);
		}
		return result;
	}
}

boogie::Expr::Ref BoogieContext::bvExtract(boogie::Expr::Ref expr, unsigned exprSize, unsigned high, unsigned low)
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
		boogie::TypeDeclRef resultType = intType(resultSize);
		boogie::TypeDeclRef exprType = intType(exprSize);

		// Boogie declaration
		boogie::FuncDeclRef fnDecl = boogie::Decl::function(
				fnNameSS.str(),     // Name
				{ { boogie::Expr::id(""), exprType} }, // Arguments
				resultType,         // Type
				nullptr,            // Body = null
				{ boogie::Attr::attr("bvbuiltin", fnSmtSS.str()) } // Attributes
		);

		// Add it
		addBuiltinFunction(fnDecl);
	}

	return boogie::Expr::fn(fnName, expr);
}

boogie::Expr::Ref BoogieContext::bvZeroExt(boogie::Expr::Ref expr, unsigned exprSize, unsigned resultSize)
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
		boogie::TypeDeclRef resultType = intType(resultSize);
		boogie::TypeDeclRef exprType = intType(exprSize);

		// Boogie declaration
		boogie::FuncDeclRef fnDecl = boogie::Decl::function(
				fnNameSS.str(),     // Name
				{ { boogie::Expr::id(""), exprType} }, // Arguments
				resultType,         // Type
				nullptr,            // Body = null
				{ boogie::Attr::attr("bvbuiltin", fnSmtSS.str()) } // Attributes
		);

		// Add it
		addBuiltinFunction(fnDecl);
	}

	return boogie::Expr::fn(fnName, expr);
}

boogie::Expr::Ref BoogieContext::bvSignExt(boogie::Expr::Ref expr, unsigned exprSize, unsigned resultSize)
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
		boogie::TypeDeclRef resultType = intType(resultSize);
		boogie::TypeDeclRef exprType = intType(exprSize);

		// Boogie declaration
		boogie::FuncDeclRef fnDecl = boogie::Decl::function(
				fnNameSS.str(),     // Name
				{ { boogie::Expr::id(""), exprType} }, // Arguments
				resultType,         // Type
				nullptr,            // Body = null
				{ boogie::Attr::attr("bvbuiltin", fnSmtSS.str()) } // Attributes
		);

		// Add it
		addBuiltinFunction(fnDecl);
	}

	return boogie::Expr::fn(fnName, expr);
}

boogie::Expr::Ref BoogieContext::bvNeg(unsigned bits, boogie::Expr::Ref expr)
{
	return bvUnaryOp("neg", bits, expr);
}

boogie::Expr::Ref BoogieContext::bvNot(unsigned bits, boogie::Expr::Ref expr)
{
	return bvUnaryOp("not", bits, expr);
}


boogie::Expr::Ref BoogieContext::bvAdd(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("add", bits, lhs, rhs);
}

boogie::Expr::Ref BoogieContext::bvSub(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("sub", bits, lhs, rhs);
}

boogie::Expr::Ref BoogieContext::bvMul(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("mul", bits, lhs, rhs);
}

boogie::Expr::Ref BoogieContext::bvSDiv(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("sdiv", bits, lhs, rhs);
}

boogie::Expr::Ref BoogieContext::bvUDiv(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("udiv", bits, lhs, rhs);
}

boogie::Expr::Ref BoogieContext::bvAnd(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("and", bits, lhs, rhs);
}

boogie::Expr::Ref BoogieContext::bvOr(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("or", bits, lhs, rhs);
}

boogie::Expr::Ref BoogieContext::bvXor(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("xor", bits, lhs, rhs);
}

boogie::Expr::Ref BoogieContext::bvAShr(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("ashr", bits, lhs, rhs);
}

boogie::Expr::Ref BoogieContext::bvLShr(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("lshr", bits, lhs, rhs);
}

boogie::Expr::Ref BoogieContext::bvShl(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("shl", bits, lhs, rhs);
}

boogie::Expr::Ref BoogieContext::bvSlt(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("slt", bits, lhs, rhs, boolType());
}

boogie::Expr::Ref BoogieContext::bvUlt(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("ult", bits, lhs, rhs, boolType());
}

boogie::Expr::Ref BoogieContext::bvSgt(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("sgt", bits, lhs, rhs, boolType());
}

boogie::Expr::Ref BoogieContext::bvUgt(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("ugt", bits, lhs, rhs, boolType());
}

boogie::Expr::Ref BoogieContext::bvSle(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("sle", bits, lhs, rhs, boolType());
}

boogie::Expr::Ref BoogieContext::bvUle(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("ule", bits, lhs, rhs, boolType());
}

boogie::Expr::Ref BoogieContext::bvSge(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("sge", bits, lhs, rhs, boolType());
}

boogie::Expr::Ref BoogieContext::bvUge(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs)
{
	return bvBinaryOp("uge", bits, lhs, rhs, boolType());
}

boogie::Expr::Ref BoogieContext::bvBinaryOp(std::string name, unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs, boogie::TypeDeclRef resultType)
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
		boogie::TypeDeclRef exprType = intType(bits);

		// Boogie declaration
		boogie::FuncDeclRef fnDecl = boogie::Decl::function(
				fnNameSS.str(),     // Name
				{ { boogie::Expr::id(""), exprType }, { boogie::Expr::id(""), exprType } }, // Arguments
				resultType,         // Type
				nullptr,            // Body = null
				{ boogie::Attr::attr("bvbuiltin", fnSmtSS.str()) } // Attributes
		);

		// Add it
		addBuiltinFunction(fnDecl);
	}

	return boogie::Expr::fn(fnName, lhs, rhs);
}

boogie::Expr::Ref BoogieContext::bvUnaryOp(std::string name, unsigned bits, boogie::Expr::Ref expr)
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
		boogie::TypeDeclRef exprType = intType(bits);

		// Boogie declaration
		boogie::FuncDeclRef fnDecl = boogie::Decl::function(
				fnNameSS.str(),       // Name
				{ { boogie::Expr::id(""), exprType } }, // Arguments
				exprType,  // Type
				nullptr,              // Body = null
				{ boogie::Attr::attr("bvbuiltin", fnSmtSS.str()) } // Attributes
		);

		// Add it
		addBuiltinFunction(fnDecl);
	}

	return boogie::Expr::fn(fnName, expr);
}

}
}
