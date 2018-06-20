#pragma once

#include <libsolidity/ast/ASTVisitor.h>

namespace dev
{
namespace solidity
{

class ASTBoogieConverter : public ASTConstVisitor
{
public:
	void convert(ASTNode const& _node);

	bool visitNode(ASTNode const&) override;

};

}
}
