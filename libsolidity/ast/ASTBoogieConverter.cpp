#include <libsolidity/ast/ASTBoogieConverter.h>
#include <libsolidity/ast/BoogieAst.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;
using namespace smack;

namespace dev
{
namespace solidity
{

void ASTBoogieConverter::convert(ASTNode const& _node)
{
	_node.accept(*this);
}

void ASTBoogieConverter::print(ostream& _stream)
{
	program.print(_stream);
}

bool ASTBoogieConverter::visit(SourceUnit const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(PragmaDirective const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ImportDirective const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ContractDefinition const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(InheritanceSpecifier const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(UsingForDirective const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(StructDefinition const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(EnumDefinition const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(EnumValue const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ParameterList const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(FunctionDefinition const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(VariableDeclaration const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ModifierDefinition const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ModifierInvocation const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(EventDefinition const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ElementaryTypeName const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(UserDefinedTypeName const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(FunctionTypeName const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Mapping const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ArrayTypeName const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(InlineAssembly const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Block const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(PlaceholderStatement const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(IfStatement const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(WhileStatement const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ForStatement const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Continue const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Break const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Return const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Throw const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(EmitStatement const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(VariableDeclarationStatement const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ExpressionStatement const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Conditional const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Assignment const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(TupleExpression const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(UnaryOperation const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(BinaryOperation const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(FunctionCall const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(NewExpression const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(MemberAccess const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(IndexAccess const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Identifier const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ElementaryTypeNameExpression const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Literal const& _node)
{
    return visitNode(_node);
}

bool ASTBoogieConverter::visitNode(ASTNode const&)
{
	cout << "Warning: unhandled node" << endl;
	return true;
}

}
}
