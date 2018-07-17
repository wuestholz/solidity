#include <algorithm>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <libsolidity/analysis/NameAndTypeResolver.h>
#include <libsolidity/ast/ASTBoogieConverter.h>
#include <libsolidity/ast/ASTBoogieExpressionConverter.h>
#include <libsolidity/ast/ASTBoogieUtils.h>
#include <libsolidity/ast/ASTPrinter.h> // TODO: just for debugging, remove later
#include <libsolidity/ast/BoogieAst.h>
#include <libsolidity/interface/Exceptions.h>
#include <libsolidity/parsing/Parser.h>
#include <libsolidity/parsing/Scanner.h>
#include <utility>

using namespace std;
using namespace dev;
using namespace dev::solidity;

namespace dev
{
namespace solidity
{

void ASTBoogieConverter::addGlobalComment(string str)
{
	m_program.getDeclarations().push_back(smack::Decl::comment("", str));
}

const smack::Expr* ASTBoogieConverter::convertExpression(Expression const& _node)
{
	ASTBoogieExpressionConverter::Result result = ASTBoogieExpressionConverter(m_errorReporter).convert(_node);

	for (auto d : result.newDecls) { m_localDecls.push_back(d); }
	for (auto s : result.newStatements) { m_currentBlocks.top()->addStmt(s); }
	for (auto c : result.newConstants)
	{
		bool alreadyDefined = false;
		for (auto d : m_program.getDeclarations())
		{
			if (d->getName() == c->getName())
			{
				// TODO: check that other fields are equal
				alreadyDefined = true;
				break;
			}
		}
		if (!alreadyDefined) m_program.getDeclarations().push_back(c);
	}

	return result.expr;
}

void ASTBoogieConverter::createDefaultConstructor(ContractDefinition const& _node)
{
	addGlobalComment("");
	addGlobalComment("Default constructor");

	// Input parameters
	list<smack::Binding> params;
	// Add some extra parameters for globally available variables
	params.push_back(make_pair(ASTBoogieUtils::BOOGIE_THIS, ASTBoogieUtils::BOOGIE_ADDRESS_TYPE)); // this
	params.push_back(make_pair(ASTBoogieUtils::BOOGIE_MSG_SENDER, ASTBoogieUtils::BOOGIE_ADDRESS_TYPE)); // msg.sender
	params.push_back(make_pair(ASTBoogieUtils::BOOGIE_MSG_VALUE, "int")); // msg.value

	list<smack::Block*> blocks;
	blocks.push_back(smack::Block::block());
	// State variable initializers should go to the beginning of the constructor
	for (auto i : m_stateVarInitializers) (*blocks.begin())->addStmt(i);
	m_stateVarInitializers.clear(); // Clear list so that we know that initializers have been included

	string funcName = ASTBoogieUtils::BOOGIE_CONSTRUCTOR + "#" + toString(_node.id());

	// Create the procedure
	m_program.getDeclarations().push_back(smack::Decl::procedure(funcName,
			params, std::list<smack::Binding>(), std::list<smack::Decl*>(), blocks));
}

ASTBoogieConverter::ASTBoogieConverter(ErrorReporter& errorReporter, std::shared_ptr<GlobalContext> globalContext,
		std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>> scopes) :
				m_errorReporter(errorReporter),
				m_globalContext(globalContext),
				m_scopes(scopes),
				m_currentRet(nullptr),
				nextReturnLabelId(0)
{
	// Initialize global declarations
	addGlobalComment("Global declarations and definitions related to the address type");
	// address type
	m_program.getDeclarations().push_back(smack::Decl::typee(ASTBoogieUtils::BOOGIE_ADDRESS_TYPE));
	// address.balance
	m_program.getDeclarations().push_back(smack::Decl::variable(ASTBoogieUtils::BOOGIE_BALANCE, "[" + ASTBoogieUtils::BOOGIE_ADDRESS_TYPE + "]int"));
	// address.transfer()
	m_program.getDeclarations().push_back(ASTBoogieUtils::createTransferProc());
	// address.call()
	m_program.getDeclarations().push_back(ASTBoogieUtils::createCallProc());
	// address.send()
	m_program.getDeclarations().push_back(ASTBoogieUtils::createSendProc());

	// Uninterpreted type for strings
	m_program.getDeclarations().push_back(smack::Decl::typee(ASTBoogieUtils::BOOGIE_STRING_TYPE));
}

void ASTBoogieConverter::convert(ASTNode const& _node)
{
	_node.accept(*this);
}

void ASTBoogieConverter::print(ostream& _stream)
{
	m_program.print(_stream);
}

// ---------------------------------------------------------------------------
//         Visitor methods for top-level nodes and declarations
// ---------------------------------------------------------------------------

bool ASTBoogieConverter::visit(SourceUnit const& _node)
{
	// Boogie programs are flat, source units do not appear explicitly
	addGlobalComment("");
	addGlobalComment("------- Source: " + _node.annotation().path + " -------");
	return true; // Simply apply visitor recursively
}

bool ASTBoogieConverter::visit(PragmaDirective const& _node)
{
	// Pragmas are only included as comments
	addGlobalComment("Pragma: " + boost::algorithm::join(_node.literals(), ""));
	return false;
}

bool ASTBoogieConverter::visit(ImportDirective const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: ImportDirective") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(ContractDefinition const& _node)
{
	// Boogie programs are flat, contracts do not appear explicitly
	addGlobalComment("");
	addGlobalComment("------- Contract: " + _node.name() + " -------");

	NameAndTypeResolver resolver(m_globalContext->declarations(), m_scopes, m_errorReporter);

	m_currentInvars.clear();
	for (auto dt : _node.annotation().docTags)
	{
		if (dt.first == "notice" && boost::starts_with(dt.second.content, "invariant "))
		{
			string invarStr = dt.second.content.substr(dt.second.content.find(" ") + 1);
			addGlobalComment("Contract invariant: " + invarStr);
			ASTPointer<Expression> invar = Parser(m_errorReporter)
					.parseExpression(shared_ptr<Scanner>(new Scanner((CharStream)invarStr)));

			m_scopes[&*invar] = m_scopes[&_node];
			resolver.resolveNamesAndTypes(*invar);

			m_currentInvars.push_back(ASTBoogieExpressionConverter(m_errorReporter).convert(*invar).expr);
			//cerr << endl << "DEBUG: AST for " << invarStr << endl; ASTPrinter(*invar).print(cerr); // TODO remove this
		}
	}


	// Process state variables first (to get initializer expressions)
	m_stateVarInitializers.clear();
	for (auto sn : _node.subNodes())
	{
		if (dynamic_cast<VariableDeclaration const*>(&*sn)) { sn->accept(*this); }
	}

	// Process other elements
	for (auto sn : _node.subNodes())
	{
		if (!dynamic_cast<VariableDeclaration const*>(&*sn)) { sn->accept(*this); }
	}

	// If there are still initializers left, there was no constructor
	if (!m_stateVarInitializers.empty()) { createDefaultConstructor(_node); }

	return false;
}

bool ASTBoogieConverter::visit(InheritanceSpecifier const& _node)
{
	// TODO: calling constructor of superclass?
	// Boogie programs are flat, inheritance does not appear explicitly
	addGlobalComment("Inherits from: " + boost::algorithm::join(_node.name().namePath(), "#"));
	if (_node.arguments() && _node.arguments()->size() > 0)
	{
		m_errorReporter.error(Error::Type::ParserError, _node.location(),
				"Boogie compiler does not support arguments for base constructor in inheritance list");
	}
	return false;
}

bool ASTBoogieConverter::visit(UsingForDirective const& _node)
{
	// Nothing to do with using for directives, calls to functions are resolved in the AST
	string libraryName = _node.libraryName().annotation().type->toString();
	string typeName = _node.typeName() ? _node.typeName()->annotation().type->toString() : "*";
	addGlobalComment("Using " + libraryName + " for " + typeName);
	return false;
}

bool ASTBoogieConverter::visit(StructDefinition const& _node)
{
	m_errorReporter.error(Error::Type::ParserError, _node.location(), "Boogie compiler does not support struct definitions");
	return false;
}

bool ASTBoogieConverter::visit(EnumDefinition const& _node)
{
	m_errorReporter.error(Error::Type::ParserError, _node.location(), "Boogie compiler does not support enum definitions");
	return false;
}

bool ASTBoogieConverter::visit(EnumValue const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: EnumValue") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(ParameterList const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: ParameterList") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(FunctionDefinition const& _node)
{
	m_currentFunc = &_node;
	// Solidity functions are mapped to Boogie procedures
	addGlobalComment("");
	string funcType = _node.visibility() == Declaration::Visibility::External ? "" : " : " + _node.type()->toString();
	addGlobalComment("Function: " + _node.name() + funcType);

	// Input parameters
	list<smack::Binding> params;
	// Add some extra parameters for globally available variables
	if (!(_node.inContractKind() == ContractDefinition::ContractKind::Library))
	{
		params.push_back(make_pair(ASTBoogieUtils::BOOGIE_THIS, ASTBoogieUtils::BOOGIE_ADDRESS_TYPE)); // this
	}
	params.push_back(make_pair(ASTBoogieUtils::BOOGIE_MSG_SENDER, ASTBoogieUtils::BOOGIE_ADDRESS_TYPE)); // msg.sender
	params.push_back(make_pair(ASTBoogieUtils::BOOGIE_MSG_VALUE, "int")); // msg.value
	// Add original parameters of the function
	for (auto p : _node.parameters())
	{
		params.push_back(make_pair(ASTBoogieUtils::mapDeclName(*p), ASTBoogieUtils::mapType(p->type(), *p, m_errorReporter)));
		if (p->type()->category() == Type::Category::Array) // Array length
		{
			params.push_back(make_pair(ASTBoogieUtils::mapDeclName(*p) + ASTBoogieUtils::BOOGIE_LENGTH, "int"));
		}
	}

	// Return values
	list<smack::Binding> rets;
	for (auto p : _node.returnParameters())
	{
		if (p->type()->category() == Type::Category::Array)
		{
			m_errorReporter.error(Error::Type::ParserError, _node.location(), "Boogie compiler does not support arrays as return values");
			return false;
		}
		rets.push_back(make_pair(ASTBoogieUtils::mapDeclName(*p), ASTBoogieUtils::mapType(p->type(), *p, m_errorReporter)));
	}

	// Boogie treats return as an assignment to the return variable(s)
	if (_node.returnParameters().empty())
	{
		m_currentRet = nullptr;
	}
	else if (_node.returnParameters().size() == 1)
	{
		m_currentRet = smack::Expr::id(rets.begin()->first);
	}
	else
	{
		m_errorReporter.error(Error::Type::ParserError, _node.location(), "Boogie compiler does not support multiple return values");
		return false;
	}

	// Convert function body, collect result
	m_localDecls.clear();
	// Create new empty block
	m_currentBlocks.push(smack::Block::block());
	// State variable initializers should go to the beginning of the constructor
	if (_node.isConstructor())
	{
		for (auto i : m_stateVarInitializers) m_currentBlocks.top()->addStmt(i);
		m_stateVarInitializers.clear(); // Clear list so that we know that initializers have been included
	}
	if (_node.modifiers().empty())
	{
		if (_node.isImplemented())
		{
			string retLabel = "$return" + to_string(nextReturnLabelId);
			++nextReturnLabelId;
			m_currentReturnLabel = retLabel;
			_node.body().accept(*this);
			m_currentBlocks.top()->addStmt(smack::Stmt::label(retLabel));
		}
	}
	else
	{
		m_currentModifier = 0;
		auto modifier = _node.modifiers()[m_currentModifier];
		auto modifierDecl = dynamic_cast<ModifierDefinition const*>(modifier->name()->annotation().referencedDeclaration);

		if (modifierDecl)
		{
			string retLabel = "$return" + to_string(nextReturnLabelId);
			++nextReturnLabelId;
			m_currentReturnLabel = retLabel;
			m_currentBlocks.top()->addStmt(smack::Stmt::comment("Inlined modifier " + modifierDecl->name() + " starts here"));

			if (modifier->arguments())
			{
				for (unsigned long i = 0; i < modifier->arguments()->size(); ++i)
				{
					smack::Decl* modifierParam = smack::Decl::variable(ASTBoogieUtils::mapDeclName(*(modifierDecl->parameters()[i])),
							ASTBoogieUtils::mapType(modifierDecl->parameters()[i]->annotation().type, *(modifierDecl->parameters()[i]), m_errorReporter));
					m_localDecls.push_back(modifierParam);
					smack::Expr const* modifierArg = convertExpression(*modifier->arguments()->at(i));
					m_currentBlocks.top()->addStmt(smack::Stmt::assign(smack::Expr::id(modifierParam->getName()), modifierArg));
				}
			}

			modifierDecl->body().accept(*this);
			m_currentBlocks.top()->addStmt(smack::Stmt::label(retLabel));
			m_currentBlocks.top()->addStmt(smack::Stmt::comment("Inlined modifier " + modifierDecl->name() + " ends here"));
		}
		else
		{
			m_errorReporter.error(Error::Type::ParserError, modifier->location(), "Unsupported modifier invocation by Boogie compiler");
		}
	}

	list<smack::Block*> blocks;
	if (!m_currentBlocks.top()->getStatements().empty()) { blocks.push_back(m_currentBlocks.top()); }
	m_currentBlocks.pop();
	if (!m_currentBlocks.empty())
	{
		BOOST_THROW_EXCEPTION(InternalCompilerError() <<
				errinfo_comment("Non-empty stack of blocks at the end of function."));
	}

	// Get the name of the function
	string funcName = _node.isConstructor() ?
			ASTBoogieUtils::BOOGIE_CONSTRUCTOR + "#" + toString(_node.id()) : // TODO: we should use the ID of the contract (the default constructor can only use that)
			ASTBoogieUtils::mapDeclName(_node);

	// Create the procedure
	auto procDecl = smack::Decl::procedure(funcName, params, rets, m_localDecls, blocks);
	for (auto invar : m_currentInvars)
	{
		if (!_node.isConstructor()) {procDecl->getRequires().push_back(invar); }
		procDecl->getEnsures().push_back(invar);
	}
	m_program.getDeclarations().push_back(procDecl);
	return false;
}

bool ASTBoogieConverter::visit(VariableDeclaration const& _node)
{
	// Non-state variables should be handled in the VariableDeclarationStatement
	if (!_node.isStateVariable())
	{
		BOOST_THROW_EXCEPTION(InternalCompilerError() <<
				errinfo_comment("Non-state variable appearing in VariableDeclaration") <<
				errinfo_sourceLocation(_node.location()));
	}
	if (_node.value())
	{
		// Initialization is saved for the constructor
		// A new block is introduced to collect side effects of the expression
		m_currentBlocks.push(smack::Block::block());
		smack::Expr const* initExpr = convertExpression(*_node.value());
		for (auto stmt : *m_currentBlocks.top()) { m_stateVarInitializers.push_back(stmt); }
		m_currentBlocks.pop();
		m_stateVarInitializers.push_back(smack::Stmt::assign(smack::Expr::id(ASTBoogieUtils::mapDeclName(_node)),
				smack::Expr::upd(
						smack::Expr::id(ASTBoogieUtils::mapDeclName(_node)),
						smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS),
						initExpr)));
	}

	addGlobalComment("");
	addGlobalComment("State variable: " + _node.name() + " : " + _node.type()->toString());
	// State variables are represented as maps from address to their type
	m_program.getDeclarations().push_back(
			smack::Decl::variable(ASTBoogieUtils::mapDeclName(_node),
			"[" + ASTBoogieUtils::BOOGIE_ADDRESS_TYPE + "]" + ASTBoogieUtils::mapType(_node.type(), _node, m_errorReporter)));

	// Arrays require an extra variable for their length
	if (_node.type()->category() == Type::Category::Array)
	{
		m_program.getDeclarations().push_back(
				smack::Decl::variable(ASTBoogieUtils::mapDeclName(_node) + ASTBoogieUtils::BOOGIE_LENGTH,
				"[" + ASTBoogieUtils::BOOGIE_ADDRESS_TYPE + "]int"));
	}
	return false;
}

bool ASTBoogieConverter::visit(ModifierDefinition const&)
{
	// Modifier definitions do not appear explicitly, but are instead inlined to functions
	return false;
}

bool ASTBoogieConverter::visit(ModifierInvocation const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: ModifierInvocation") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(EventDefinition const& _node)
{
	m_errorReporter.warning(_node.location(), "Ignored event definition");
	return false;
}

bool ASTBoogieConverter::visit(ElementaryTypeName const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: ElementaryTypeName") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(UserDefinedTypeName const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: UserDefinedTypeName") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(FunctionTypeName const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: FunctionTypeName") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(Mapping const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: Mapping") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(ArrayTypeName const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: ArrayTypeName") << errinfo_sourceLocation(_node.location()));
	return false;
}

// ---------------------------------------------------------------------------
//                     Visitor methods for statements
// ---------------------------------------------------------------------------

bool ASTBoogieConverter::visit(InlineAssembly const& _node)
{
	m_errorReporter.error(Error::Type::ParserError, _node.location(), "Boogie compiler does not support inline assembly");
	return false;
}

bool ASTBoogieConverter::visit(Block const&)
{
	// Simply apply visitor recursively, compound statements will
	// handle creating new blocks when required
	return true;
}

bool ASTBoogieConverter::visit(PlaceholderStatement const&)
{
	m_currentModifier++;

	if (m_currentModifier < m_currentFunc->modifiers().size())
	{
		auto modifier = m_currentFunc->modifiers()[m_currentModifier];
		auto modifierDecl = dynamic_cast<ModifierDefinition const*>(modifier->name()->annotation().referencedDeclaration);

		if (modifierDecl)
		{
			// Duplicated modifiers currently do not work, because they will introduce
			// local variables for their parameters with the same name
			for (unsigned long i = 0; i < m_currentModifier; ++i)
			{
				if (m_currentFunc->modifiers()[i]->name()->annotation().referencedDeclaration->id() == modifierDecl->id()) {
					m_errorReporter.error(Error::Type::ParserError, m_currentFunc->location(), "Boogie compiler does not support duplicated modifiers");
				}
			}

			string oldReturnLabel = m_currentReturnLabel;
			m_currentReturnLabel = "$return" + to_string(nextReturnLabelId);
			++nextReturnLabelId;
			m_currentBlocks.top()->addStmt(smack::Stmt::comment("Inlined modifier " + modifierDecl->name() + " starts here"));
			if (modifier->arguments())
			{
				for (unsigned long i = 0; i < modifier->arguments()->size(); ++i)
				{
					smack::Decl* modifierParam = smack::Decl::variable(ASTBoogieUtils::mapDeclName(*(modifierDecl->parameters()[i])),
							ASTBoogieUtils::mapType(modifierDecl->parameters()[i]->annotation().type, *(modifierDecl->parameters()[i]), m_errorReporter));
					m_localDecls.push_back(modifierParam);
					smack::Expr const* modifierArg = convertExpression(*modifier->arguments()->at(i));
					m_currentBlocks.top()->addStmt(smack::Stmt::assign(smack::Expr::id(modifierParam->getName()), modifierArg));
				}
			}
			modifierDecl->body().accept(*this);
			m_currentBlocks.top()->addStmt(smack::Stmt::label(m_currentReturnLabel));
			m_currentBlocks.top()->addStmt(smack::Stmt::comment("Inlined modifier " + modifierDecl->name() + " ends here"));
			m_currentReturnLabel = oldReturnLabel;
		}
		else
		{
			m_errorReporter.error(Error::Type::ParserError, modifier->location(), "Unsupported modifier invocation by Boogie compiler");
		}
	}
	else if (m_currentFunc->isImplemented())
	{
		string oldReturnLabel = m_currentReturnLabel;
		m_currentReturnLabel = "$return" + to_string(nextReturnLabelId);
		++nextReturnLabelId;
		m_currentBlocks.top()->addStmt(smack::Stmt::comment("Function body starts here"));
		m_currentFunc->body().accept(*this);
		m_currentBlocks.top()->addStmt(smack::Stmt::label(m_currentReturnLabel));
		m_currentBlocks.top()->addStmt(smack::Stmt::comment("Function body ends here"));
		m_currentReturnLabel = oldReturnLabel;
	}

	m_currentModifier--;

	return false;
}

bool ASTBoogieConverter::visit(IfStatement const& _node)
{
	// Get condition recursively
	const smack::Expr* cond = convertExpression(_node.condition());

	// Get true branch recursively
	m_currentBlocks.push(smack::Block::block());
	_node.trueStatement().accept(*this);
	const smack::Block* thenBlock = m_currentBlocks.top();
	m_currentBlocks.pop();

	// Get false branch recursively
	const smack::Block* elseBlock = nullptr;
	if (_node.falseStatement())
	{
		m_currentBlocks.push(smack::Block::block());
		_node.falseStatement()->accept(*this);
		elseBlock = m_currentBlocks.top();
		m_currentBlocks.pop();
	}

	m_currentBlocks.top()->addStmt(smack::Stmt::ifelse(cond, thenBlock, elseBlock));
	return false;
}

bool ASTBoogieConverter::visit(WhileStatement const& _node)
{
	// Get condition recursively
	const smack::Expr* cond = convertExpression(_node.condition());

	// Get if branch recursively
	m_currentBlocks.push(smack::Block::block());
	_node.body().accept(*this);
	const smack::Block* body = m_currentBlocks.top();
	m_currentBlocks.pop();

	// TODO: loop invariants can be added here

	m_currentBlocks.top()->addStmt(smack::Stmt::while_(cond, body));

	return false;
}

bool ASTBoogieConverter::visit(ForStatement const& _node)
{
	// Boogie does not have a for statement, therefore it is transformed
	// into a while statement in the following way:
	//
	// for (initExpr; cond; loopExpr) { body }
	//
	// initExpr; while (cond) { body; loopExpr }

	// Get initialization recursively (adds statement to current block)
	m_currentBlocks.top()->addStmt(smack::Stmt::comment("The following while loop was mapped from a for loop"));
	if (_node.initializationExpression())
	{
		_node.initializationExpression()->accept(*this);
	}

	// Get condition recursively
	const smack::Expr* cond = nullptr;
	if (_node.condition())
	{
		cond = convertExpression(*_node.condition());
	}

	// Get body recursively
	m_currentBlocks.push(smack::Block::block());
	_node.body().accept(*this);
	// Include loop expression at the end of body
	if (_node.loopExpression())
	{
		_node.loopExpression()->accept(*this); // Adds statement to current block
	}
	const smack::Block* body = m_currentBlocks.top();
	m_currentBlocks.pop();

	// TODO: loop invariants can be added here

	m_currentBlocks.top()->addStmt(smack::Stmt::while_(cond, body));

	return false;
}

bool ASTBoogieConverter::visit(Continue const& _node)
{
	// TODO: Boogie does not support continue, this must be mapped manually
	// using labels and gotos
	m_errorReporter.error(Error::Type::ParserError, _node.location(), "Boogie compiler does not support continue statement");
	return false;
}

bool ASTBoogieConverter::visit(Break const&)
{
	m_currentBlocks.top()->addStmt(smack::Stmt::break_());
	return false;
}

bool ASTBoogieConverter::visit(Return const& _node)
{
	// No return value
	if (_node.expression() != nullptr)
	{
		// Get rhs recursively
		const smack::Expr* rhs = convertExpression(*_node.expression());

		// lhs should already be known (set by the enclosing FunctionDefinition)
		const smack::Expr* lhs = m_currentRet;

		// First create an assignment, and then an empty return
		m_currentBlocks.top()->addStmt(smack::Stmt::assign(lhs, rhs));
	}
	list<string> label;
	label.push_back(m_currentReturnLabel);
	m_currentBlocks.top()->addStmt(smack::Stmt::goto_(label));
	return false;
}

bool ASTBoogieConverter::visit(Throw const&)
{
	m_currentBlocks.top()->addStmt(smack::Stmt::assume(smack::Expr::lit(false)));
	return false;
}

bool ASTBoogieConverter::visit(EmitStatement const& _node)
{
	m_errorReporter.warning(_node.location(), "Ignored emit statement");
	return false;
}

bool ASTBoogieConverter::visit(VariableDeclarationStatement const& _node)
{
	for (auto decl : _node.declarations())
	{
		if (decl->isLocalVariable())
		{
			// Boogie requires local variables to be declared at the beginning of the procedure
			m_localDecls.push_back(smack::Decl::variable(
					ASTBoogieUtils::mapDeclName(*decl),
					ASTBoogieUtils::mapType(decl->type(), *decl, m_errorReporter)));
		}
		else
		{
			// Non-local variables should be handled elsewhere
			BOOST_THROW_EXCEPTION(InternalCompilerError() <<
					errinfo_comment("Non-local variable appearing in VariableDeclarationStatement") <<
					errinfo_sourceLocation(_node.location()));
		}
	}

	// Convert initial value into an assignment statement
	if (_node.initialValue())
	{
		if (_node.declarations().size() == 1)
		{
			// Get expression recursively
			const smack::Expr* rhs = convertExpression(*_node.initialValue());

			m_currentBlocks.top()->addStmt(smack::Stmt::assign(
					smack::Expr::id(ASTBoogieUtils::mapDeclName(**_node.declarations().begin())),
					rhs));
		}
		else
		{
			m_errorReporter.error(Error::Type::ParserError, _node.location(), "Boogie compiler does not support assignment to multiple variables");
		}
	}
	return false;
}

bool ASTBoogieConverter::visit(ExpressionStatement const& _node)
{
	// Some expressions have specific statements in Boogie

	// Assignment
	if (dynamic_cast<Assignment const*>(&_node.expression()))
	{
		convertExpression(_node.expression());
		return false;
	}

	// Function call
	if (dynamic_cast<FunctionCall const*>(&_node.expression()))
	{
		convertExpression(_node.expression());
		return false;
	}

	// Increment, decrement
	if (auto unaryExpr = dynamic_cast<UnaryOperation const*>(&_node.expression()))
	{
		if (unaryExpr->getOperator() == Token::Inc || unaryExpr->getOperator() == Token::Dec)
		{
			convertExpression(_node.expression());
			return false;
		}
	}

	// Other statements are assigned to a temp variable because Boogie
	// does not allow stand alone expressions

	smack::Expr const* expr = convertExpression(_node.expression());
	smack::Decl* tmpVar = smack::Decl::variable("tmpVar" + to_string(_node.id()),
			ASTBoogieUtils::mapType(_node.expression().annotation().type, _node, m_errorReporter));
	m_localDecls.push_back(tmpVar);
	m_currentBlocks.top()->addStmt(smack::Stmt::comment("Assignment to temp variable introduced because Boogie does not support stand alone expressions"));
	m_currentBlocks.top()->addStmt(smack::Stmt::assign(smack::Expr::id(tmpVar->getName()), expr));
	return false;
}


bool ASTBoogieConverter::visitNode(ASTNode const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node (unknown)") << errinfo_sourceLocation(_node.location()));
	return true;
}

}
}
