#include <boost/algorithm/string/predicate.hpp>
#include <libsolidity/ast/ASTBoogieStats.h>
#include <libsolidity/ast/ASTBoogieUtils.h>

using namespace dev;
using namespace dev::solidity;

namespace dev
{
namespace solidity
{

bool ASTBoogieStats::hasDocTag(DocumentedAnnotation const& _annot, std::string _tag) const
{
	for (auto docTag: _annot.docTags)
		if (docTag.first == "notice" && boost::starts_with(docTag.second.content, _tag)) // Find expressions with the given tag
			return true;

	return false;
}

bool ASTBoogieStats::visit(ContractDefinition const& _node)
{
	for (auto cn: _node.annotation().linearizedBaseContracts)
	{
		for (auto structDef: cn->definedStructs())
		{
			if (m_contractsForStructs.find(structDef) == m_contractsForStructs.end())
				m_contractsForStructs[structDef] = {};

			m_contractsForStructs[structDef].push_back(&_node);
		}
	}
	return true;
}

bool ASTBoogieStats::visit(FunctionDefinition const& _node)
{
	if (!m_hasModifiesSpecs)
	{
		m_hasModifiesSpecs = m_hasModifiesSpecs || hasDocTag(_node.annotation(), ASTBoogieUtils::DOCTAG_MODIFIES);
		m_hasModifiesSpecs = m_hasModifiesSpecs || hasDocTag(_node.annotation(), ASTBoogieUtils::DOCTAG_MODIFIES_ALL);
	}
	return true;
}

}

}
