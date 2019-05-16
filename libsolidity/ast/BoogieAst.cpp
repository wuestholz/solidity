//
// This file is distributed under the MIT License. See SMACK-LICENSE for details.
//
#include <libsolidity/ast/BoogieAst.h>
#include <sstream>
#include <iostream>
#include <set>
#include <cassert>

namespace boogie {

unsigned Decl::uniqueId = 0;

Expr::Ref Expr::exists(std::vector<Binding> const& vars, Ref e)
{
	return std::make_shared<QuantExpr const>(QuantExpr::Exists, vars, e);
}

Expr::Ref Expr::forall(std::vector<Binding> const& vars, Ref e)
{
	return std::make_shared<QuantExpr const>(QuantExpr::Forall, vars, e);
}

Expr::Ref Expr::and_(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::And, l, r);
}

Expr::Ref Expr::or_(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Or, l, r);
}

Expr::Ref Expr::cond(Ref c, Ref t, Ref e)
{
	return std::make_shared<CondExpr const>(c,t,e);
}

Expr::Ref Expr::eq(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Eq, l, r);
}

Expr::Ref Expr::lt(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Lt, l, r);
}

Expr::Ref Expr::gt(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Gt, l, r);
}

Expr::Ref Expr::lte(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Lte, l, r);
}

Expr::Ref Expr::gte(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Gte, l, r);
}

Expr::Ref Expr::plus(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Plus, l, r);
}

Expr::Ref Expr::minus(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Minus, l, r);
}

Expr::Ref Expr::div(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Div, l, r);
}

Expr::Ref Expr::intdiv(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::IntDiv, l, r);
}

Expr::Ref Expr::times(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Times, l, r);
}

Expr::Ref Expr::mod(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Mod, l, r);
}

Expr::Ref Expr::exp(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Exp, l, r);
}

Expr::Ref Expr::fn(std::string f, std::vector<Ref> const& args)
{
	return std::make_shared<FunExpr const>(f, args);
}

Expr::Ref Expr::fn(std::string f, Ref x)
{
	return std::make_shared<FunExpr const>(f, std::vector<Ref>{x});
}

Expr::Ref Expr::fn(std::string f, Ref x, Ref y)
{
	return std::make_shared<FunExpr const>(f, std::vector<Ref>{x, y});
}

Expr::Ref Expr::fn(std::string f, Ref x, Ref y, Ref z)
{
	return std::make_shared<FunExpr const>(f, std::vector<Ref>{x, y, z});
}

Expr::Ref Expr::id(std::string s)
{
	return std::make_shared<VarExpr const>(s);
}

Expr::Ref Expr::impl(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Imp, l, r);
}

Expr::Ref Expr::iff(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Iff, l, r);
}

Expr::Ref Expr::lit(bool b)
{
	return std::make_shared<BoolLit const>(b);
}

Expr::Ref Expr::lit(std::string v)
{
	return std::make_shared<StringLit const>(v);
}

Expr::Ref Expr::lit(unsigned long v)
{
	return std::make_shared<IntLit const>(v);
}

Expr::Ref Expr::lit(long v)
{
	return std::make_shared<IntLit const>(v);
}

Expr::Ref Expr::lit(bigint v)
{
	return std::make_shared<IntLit const>(v);
}

Expr::Ref Expr::lit(std::string v, unsigned w)
{
	return w ? (Ref) new BvLit(v,w) : (Ref) new IntLit(v);
}

Expr::Ref Expr::lit(bigint v, unsigned w)
{
	return std::make_shared<BvLit const>(v,w);
}

Expr::Ref Expr::lit(bool n, std::string s, std::string e, unsigned ss, unsigned es)
{
	return std::make_shared<FPLit const>(n, s, e, ss, es);
}

Expr::Ref Expr::neq(Ref l, Ref r)
{
	return std::make_shared<BinExpr const>(BinExpr::Neq, l, r);
}

Expr::Ref Expr::not_(Ref e)
{
	return std::make_shared<NotExpr const>(e);
}

Expr::Ref Expr::neg(Ref e)
{
	return std::make_shared<NegExpr const>(e);
}

Expr::Ref Expr::sel(Ref b, Ref i)
{
	return std::make_shared<SelExpr const>(b, i);
}

Expr::Ref Expr::sel(std::string b, std::string i)
{
	return std::make_shared<SelExpr const>(id(b), id(i));
}

Expr::Ref Expr::sel(Ref b, std::vector<Ref> const& i)
{
	return std::make_shared<SelExpr const>(b, i);
}

Expr::Ref Expr::upd(Ref b, Ref i, Ref v)
{
	return std::make_shared<UpdExpr const>(b, i, v);
}

Expr::Ref Expr::upd(Ref b, std::vector<Ref> const& i, Ref v)
{
	return std::make_shared<UpdExpr const>(b, i, v);
}

Expr::Ref Expr::if_then_else(Ref c, Ref t, Ref e)
{
	return std::make_shared<IfThenElseExpr const>(c, t, e);
}

Expr::Ref Expr::old(Ref expr)
{
	return std::make_shared<OldExpr const>(expr);
}

Expr::Ref Expr::tuple(std::vector<Ref> const& e)
{
	return std::make_shared<TupleExpr const>(e);
}

Attr::Ref Attr::attr(std::string s, std::vector<Expr::Ref> const& vs)
{
	return std::make_shared<Attr const>(s,vs);
}

Attr::Ref Attr::attr(std::string s)
{
	return attr(s, {});
}

Attr::Ref Attr::attr(std::string s, std::string v)
{
	return Attr::Ref(new Attr(s, { Expr::lit(v) }));
}

Attr::Ref Attr::attr(std::string s, int v)
{
	return attr(s, {Expr::lit((long) v)});
}

Attr::Ref Attr::attr(std::string s, std::string v, int i)
{
	return attr(s, std::vector<Expr::Ref>{ Expr::lit(v), Expr::lit((long) i) });
}

Attr::Ref Attr::attr(std::string s, std::string v, int i, int j)
{
	return attr(s, {Expr::lit(v), Expr::lit((long) i), Expr::lit((long) j)});
}

Stmt::Ref Stmt::annot(std::vector<Attr::Ref> const& attrs)
{
	AssumeStmt* s = new AssumeStmt(Expr::lit(true));
	for (auto A : attrs)
		s->add(A);
	return std::shared_ptr<AssumeStmt const>(s);
}

Stmt::Ref Stmt::annot(Attr::Ref a)
{
	return Stmt::annot(std::vector<Attr::Ref>{a});
}

Stmt::Ref Stmt::assert_(Expr::Ref e, std::vector<Attr::Ref> const& attrs)
{
	return std::make_shared<AssertStmt const>(e, attrs);
}

Stmt::Ref Stmt::assign(Expr::Ref e, Expr::Ref f)
{
	return std::make_shared<AssignStmt const>(std::vector<Expr::Ref>{e}, std::vector<Expr::Ref>{f});
}

Stmt::Ref Stmt::assign(std::vector<Expr::Ref> const& lhs, std::vector<Expr::Ref> const& rhs)
{
	return std::make_shared<AssignStmt const>(lhs, rhs);
}

Stmt::Ref Stmt::assume(Expr::Ref e)
{
	return std::make_shared<AssumeStmt const>(e);
}

Stmt::Ref Stmt::assume(Expr::Ref e, Attr::Ref a)
{
	AssumeStmt* s = new AssumeStmt(e);
	s->add(a);
	return std::shared_ptr<AssumeStmt const>(s);
}

Stmt::Ref Stmt::call(std::string p, std::vector<Expr::Ref> const& args, std::vector<std::string> const& rets,
		std::vector<Attr::Ref> const& attrs)
{
	return std::make_shared<CallStmt const>(p, attrs, args, rets);
}

Stmt::Ref Stmt::comment(std::string s)
{
	return std::make_shared<Comment const>(s);
}

Stmt::Ref Stmt::goto_(std::vector<std::string> const& ts)
{
	return std::make_shared<GotoStmt const>(ts);
}

Stmt::Ref Stmt::havoc(std::string x)
{
	return std::make_shared<HavocStmt const>(std::vector<std::string>{x});
}

Stmt::Ref Stmt::return_(Expr::Ref e)
{
	return std::make_shared<ReturnStmt const>(e);
}

Stmt::Ref Stmt::return_()
{
	return std::make_shared<ReturnStmt const>();
}

Stmt::Ref Stmt::skip()
{
	return std::make_shared<AssumeStmt const>(Expr::lit(true));
}

Stmt::Ref Stmt::code(std::string s)
{
	return std::make_shared<CodeStmt const>(s);
}

Stmt::Ref Stmt::ifelse(Expr::Ref cond, Block::ConstRef then, Block::ConstRef elze)
{
	return std::make_shared<IfElseStmt const>(cond, then, elze);
}

Stmt::Ref Stmt::while_(Expr::Ref cond, Block::ConstRef body, std::vector<Specification::Ref> const& invars)
{
	return std::make_shared<WhileStmt const>(cond, body, invars);
}

Stmt::Ref Stmt::break_()
{
	return std::make_shared<BreakStmt const>();
}

Stmt::Ref Stmt::label(std::string s)
{
	return std::make_shared<LabelStmt const>(s);
}

Decl::Ref Decl::typee(std::string name, std::string type, std::vector<Attr::Ref> const& attrs)
{
	return std::make_shared<TypeDecl>(name,type,attrs);
}
Decl::Ref Decl::axiom(Expr::Ref e, std::string name)
{
	return std::make_shared<AxiomDecl>(name, e);
}

FuncDeclRef Decl::function(std::string name, std::vector<Binding> const& args,
		std::string type, Expr::Ref e, std::vector<Attr::Ref> const& attrs)
{
	return std::make_shared<FuncDecl>(name,attrs,args,type,e);
}

Decl::Ref Decl::constant(std::string name, std::string type)
{
	return Decl::constant(name, type, {}, false);
}

Decl::Ref Decl::constant(std::string name, std::string type, bool unique)
{
	return Decl::constant(name, type, {}, unique);
}

Decl::Ref Decl::constant(std::string name, std::string type, std::vector<Attr::Ref> const& ax, bool unique)
{
	return std::make_shared<ConstDecl>(name, type, ax, unique);
}

Decl::Ref Decl::variable(std::string name, std::string type)
{
	return std::make_shared<VarDecl>(name, type);
}

ProcDeclRef Decl::procedure(std::string name,
		std::vector<Binding> const& args,
		std::vector<Binding> const& rets,
		std::vector<Decl::Ref> const& decls,
		std::vector<Block::Ref> const& blocks)
{
	return std::make_shared<ProcDecl>(name, args, rets, decls, blocks);
}
Decl::Ref Decl::code(std::string name, std::string s)
{
	return std::make_shared<CodeDecl>(name, s);
}

FuncDeclRef Decl::code(ProcDeclRef P)
{
	std::vector<Decl::Ref> decls;
	std::vector<Block::Ref> blocks;
	for (auto B : *P)
	{
		blocks.push_back(Block::block(B->getName()));
		for (auto S : *B)
		{
			Stmt::Ref SS;
			// Original version was: if (llvm::isa<ReturnStmt>(S))
			if (std::dynamic_pointer_cast<ReturnStmt const>(S))
				SS = Stmt::return_(Expr::neq(Expr::id(P->getReturns().front().id),Expr::lit(0U)));
			else
				SS = S;
			blocks.back()->getStatements().push_back(SS);
		}
	}
	for (auto D : P->getDeclarations())
		decls.push_back(D);

	// HACK to avoid spurious global-var modification
	//decls.push_back(Decl::variable(Naming::EXN_VAR, "bool"));

	for (auto R : P->getReturns())
		decls.push_back(Decl::variable(R.id, R.type));

	return Decl::function(
		P->getName(), P->getParameters(), "bool", std::make_shared<CodeExpr const>(decls, blocks),
		{Attr::attr("inline")}
	);
}
Decl::Ref Decl::comment(std::string name, std::string str)
{
	return std::make_shared<CommentDecl>(name, str);
}

std::ostream& operator<<(std::ostream& os, const Expr& e)
{
	e.print(os);
	return os;
}

std::ostream& operator<<(std::ostream& os, Expr::Ref e)
{
	e->print(os);
	return os;
}

std::ostream& operator<<(std::ostream& os, Attr::Ref a)
{
	a->print(os);
	return os;
}

std::ostream& operator<<(std::ostream& os, Stmt::Ref s)
{
	s->print(os);
	return os;
}

std::ostream& operator<<(std::ostream& os, Block::ConstRef b)
{
	b->print(os);
	return os;
}

std::ostream& operator<<(std::ostream& os, Block::Ref b)
{
	b->print(os);
	return os;
}

std::ostream& operator<<(std::ostream& os, Decl& d)
{
	d.print(os);
	return os;
}

std::ostream& operator<<(std::ostream& os, Decl::Ref d)
{
	d->print(os);
	return os;
}

std::ostream& operator<<(std::ostream& os, Decl::ConstRef d)
{
	d->print(os);
	return os;
}

std::ostream& operator<<(std::ostream& os, const Program* p)
{
	if (p == 0)
		os << "<null> Program!\n";
	else
		p->print(os);
	return os;
}
std::ostream& operator<<(std::ostream& os, const Program& p)
{
	p.print(os);
	return os;
}

std::ostream& operator<<(std::ostream& os, const Binding& p)
{
	os << p.id << ": " << p.type;
	return os;
}

template<class T>
void print_seq(std::ostream& os, std::vector<T> const& ts, std::string init, std::string sep, std::string term)
{
	os << init;
	for (auto i = ts.begin(); i != ts.end(); ++i)
		os << (i == ts.begin() ? "" : sep) << *i;
	os << term;
}

template<class T>
void print_seq(std::ostream& os, std::vector<T> const& ts, std::string sep)
{
	print_seq<T>(os, ts, "", sep, "");
}

template<class T>
void print_seq(std::ostream& os, std::vector<T> const& ts)
{
	print_seq<T>(os, ts, "", "", "");
}

template<class T, class C>
void print_set(std::ostream& os, const std::set<T,C>& ts, std::string init, std::string sep, std::string term)
{
	os << init;
	for (typename std::set<T,C>::const_iterator i = ts.begin(); i != ts.end(); ++i)
		os << (i == ts.begin() ? "" : sep) << *i;
	os << term;
}

template<class T, class C>
void print_set(std::ostream& os, const std::set<T,C>& ts, std::string sep)
{
	print_set<T,C>(os, ts, "", sep, "");
}

template<class T, class C>
void print_set(std::ostream& os, const std::set<T,C>& ts)
{
	print_set<T,C>(os, ts, "", "", "");
}

void BinExpr::print(std::ostream& os) const
{
	os << "(" << lhs << " ";
	switch (op)
	{
	case Iff:
		os << "<==>";
		break;
	case Imp:
		os << "==>";
		break;
	case Or:
		os << "||";
		break;
	case And:
		os << "&&";
		break;
	case Eq:
		os << "==";
		break;
	case Neq:
		os << "!=";
		break;
	case Lt:
		os << "<";
		break;
	case Gt:
		os << ">";
		break;
	case Lte:
		os << "<=";
		break;
	case Gte:
		os << ">=";
		break;
	case Sub:
		os << "<:";
		break;
	case Conc:
		os << "++";
		break;
	case Plus:
		os << "+";
		break;
	case Minus:
		os << "-";
		break;
	case Times:
		os << "*";
		break;
	case Div:
		os << "/";
		break;
	case IntDiv:
		os << "div";
		break;
	case Mod:
		os << "mod";
		break;
	case Exp:
		os << "**";
		break;
	}
	os << " " << rhs << ")";
}

void CondExpr::print(std::ostream& os) const
{
	os << "(if " << cond << " then " << then << " else " << else_ << ")";
}

void FunExpr::print(std::ostream& os) const
{
	os << fun;
	print_seq(os, args, "(", ", ", ")");
}

void BoolLit::print(std::ostream& os) const
{
	os << (val ? "true" : "false");
}

void IntLit::print(std::ostream& os) const
{
	os << val;
}

void BvLit::print(std::ostream& os) const
{
	os << val << "bv" << width;
}

void FPLit::print(std::ostream& os) const
{
	os << (neg ? "-" : "") << sig << "e" << expo << "f" << sigSize << "e" << expSize;
}

void NegExpr::print(std::ostream& os) const
{
	os << "-(" << expr << ")";
}

void NotExpr::print(std::ostream& os) const
{
	os << "!(" << expr << ")";
}

void QuantExpr::print(std::ostream& os) const
{
	os << "(";
	switch (quant)
	{
	case Forall:
		os << "forall ";
		break;
	case Exists:
		os << "exists ";
		break;
	}
	print_seq(os, vars, ", ");
	os << " :: " << expr << ")";
}

void SelExpr::print(std::ostream& os) const
{
	os << base;
	print_seq(os, idxs, "[", ", ", "]");
}

void UpdExpr::print(std::ostream& os) const
{
	os << base << "[";
	print_seq(os, idxs, ", ");
	os << " := " << val << "]";
}

void VarExpr::print(std::ostream& os) const
{
	os << var;
}

void OldExpr::print(std::ostream& os) const
{
	os << "old(";
	expr->print(os);
	os << ")";
}

void CodeExpr::print(std::ostream& os) const
{
	os << "|{" << "\n";
	if (decls.size() > 0)
		print_seq(os, decls, "	", "\n	", "\n");
	print_seq(os, blocks, "\n");
	os << "\n" << "}|";
}

void IfThenElseExpr::print(std::ostream& os) const
{
	os << "if " << cond << " then " << true_value << " else " << false_value;
}

void TupleExpr::print(std::ostream& os) const
{
	print_seq(os, es, ", ");
}

void StringLit::print(std::ostream& os) const
{
	os << "\"" << val << "\"";
}

void Attr::print(std::ostream& os) const
{
	os << "{:" << name;
	if (vals.size() > 0)
		print_seq(os, vals, " ", ", ", "");
	os << "}";
}

void AssertStmt::print(std::ostream& os) const
{
	os << "assert ";
	if (attrs.size() > 0)
		print_seq(os, attrs, "", " ", " ");
	os << expr << ";";
}

void AssignStmt::print(std::ostream& os) const
{
	print_seq(os, lhs, ", ");
	os << " := ";
	print_seq(os, rhs, ", ");
	os << ";";
}

void AssumeStmt::print(std::ostream& os) const
{
	os << "assume ";
	if (attrs.size() > 0)
		print_seq(os, attrs, "", " ", " ");
	os << expr << ";";
}

void CallStmt::print(std::ostream& os) const
{
	os << "call ";
	if (attrs.size() > 0)
		print_seq(os, attrs, "", " ", " ");
	if (returns.size() > 0)
		print_seq(os, returns, "", ", ", " := ");
	os << proc;
	print_seq(os, params, "(", ", ", ")");
	os << ";";
}

void Comment::print(std::ostream& os) const
{
	os << "// " << str;
}

void GotoStmt::print(std::ostream& os) const
{
	os << "goto ";
	print_seq(os, targets, ", ");
	os << ";";
}

void HavocStmt::print(std::ostream& os) const
{
	os << "havoc ";
	print_seq(os, vars, ", ");
	os << ";";
}

void ReturnStmt::print(std::ostream& os) const
{
	os << "return";
	if (expr)
		os << " " << expr;
	os << ";";
}

void CodeStmt::print(std::ostream& os) const
{
	os << code;
}

void IfElseStmt::print(std::ostream& os) const
{
	os << "if (";
	cond->print(os);
	os << ") {\n";
	then->print(os);
	os << "\n	}\n";

	if (elze)
	{
		os << "	else {\n";
		elze->print(os);
		os << "\n	}\n";
	}
}

void WhileStmt::print(std::ostream& os) const
{
	os << "while (";
	cond->print(os);
	os << ")";

	if (invars.empty())
	{
		os << " {\n";
	}
	else
	{
		os << "\n";
		for (auto inv : invars)
		{
			inv->print(os, "invariant");
			os << "\n";
		}
		os << "\n	{\n";
	}
	body->print(os);
	os << "\n	}\n";
}

void BreakStmt::print(std::ostream& os) const
{
	os << "break;";
}

void LabelStmt::print(std::ostream& os) const
{
	os << str << ":";
}

void TypeDecl::print(std::ostream& os) const
{
	os << "type ";
	if (attrs.size() > 0)
		print_seq(os, attrs, "", " ", " ");
	os << name;
	if (alias != "")
		os << " = " << alias;
	os << ";";
}

void AxiomDecl::print(std::ostream& os) const
{
	os << "axiom ";
	if (attrs.size() > 0)
		print_seq(os, attrs, "", " ", " ");
	os << expr << ";";
}

void ConstDecl::print(std::ostream& os) const
{
	os << "const ";
	if (attrs.size() > 0)
		print_seq(os, attrs, "", " ", " ");
	os << (unique ? "unique " : "") << name << ": " << type << ";";
}

void FuncDecl::print(std::ostream& os) const
{
	os << "function ";
	if (attrs.size() > 0)
		print_seq(os, attrs, "", " ", " ");
	os << name << "(";
	for (auto P = params.begin(), E = params.end(); P != E; ++P)
		os << (P == params.begin() ? "" : ", ") << (P->id != "" ? P->id + ": " : "") << P->type;
	os << ") returns (" << type << ")";
	if (body)
		os << " { " << body << " }";
	else
		os << ";";
}

void VarDecl::print(std::ostream& os) const
{
	os << "var ";
	if (attrs.size() > 0)
		print_seq(os, attrs, "", " ", " ");
	os << name << ": " << type << ";";
}

void Specification::print(std::ostream& os, std::string kind) const
{
	os << "	" << kind << " ";
	if (attrs.size() > 0)
			print_seq(os, attrs, "", " ", " ");
	os << expr << ";\n";
}

Specification::Ref Specification::spec(Expr::Ref e, std::vector<Attr::Ref> const& ax){
	return std::make_shared<Specification const>(e, ax);
}

Specification::Ref Specification::spec(Expr::Ref e){
	return Specification::spec(e, {});
}

void ProcDecl::print(std::ostream& os) const
{
	os << "procedure ";
	if (attrs.size() > 0)
		print_seq(os, attrs, "", " ", " ");
	os << name << "(";
	for (auto P = params.begin(), E = params.end(); P != E; ++P)
		os << (P == params.begin() ? "" : ", ") << P->id << ": " << P->type;
	os << ")";
	if (rets.size() > 0)
	{
		os << "\n";
		os << "	returns (";
		for (auto R = rets.begin(), E = rets.end(); R != E; ++R)
			os << (R == rets.begin() ? "" : ", ") << R->id << ": " << R->type;
		os << ")";
	}
	if (blocks.size() == 0)
		os << ";";

	if (mods.size() > 0)
	{
		os << "\n";
		print_seq(os, mods, "	modifies ", ", ", ";");
	}
	if (requires.size() > 0)
	{
		os << "\n";
		for (auto req : requires) req->print(os, "requires");
	}
	if (ensures.size() > 0)
	{
		os << "\n";
		for (auto ens : ensures) ens->print(os, "ensures");
	}
	if (blocks.size() > 0)
	{
		os << "\n";
		os << "{" << "\n";
		if (decls.size() > 0)
			print_seq(os, decls, "	", "\n	", "\n");
		print_seq(os, blocks, "\n");
		os << "\n" << "}";
	}
	os << "\n";
}

void CodeDecl::print(std::ostream& os) const
{
	os << code;
}

void CommentDecl::print(std::ostream& os) const
{
	os << "// ";
	for (char c : str)
	{
		os << c;
		if (c == '\n')
			os << "// ";
	}
}

void Block::print(std::ostream& os) const
{
	if (name != "")
		os << name << ":" << "\n";
	print_seq(os, stmts, "	", "\n	", "");
}

void Program::print(std::ostream& os) const
{
	os << prelude;
	print_seq(os, decls, "\n");
	os << "\n";
}
}
