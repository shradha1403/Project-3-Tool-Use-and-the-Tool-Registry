#include "tools/calculator.h"

#include <stack>
#include <string>
#include <vector>

namespace {

int precedence(CalcToken::Kind k) {
    switch (k) {
        case CalcToken::Kind::Times:
        case CalcToken::Kind::Divide: return 2;
        case CalcToken::Kind::Plus:
        case CalcToken::Kind::Minus:  return 1;
        default:                      return 0;
    }
}

bool is_operator(CalcToken::Kind k) { return precedence(k) > 0; }

// Pops one operator and two operands (right first, then left) and pushes the result.
bool apply_top(std::stack<double>& values, std::stack<CalcToken::Kind>& ops, std::string* error) {
    if (ops.empty() || values.size() < 2) {
        if (error) *error = "malformed expression";
        return false;
    }
    CalcToken::Kind op = ops.top();
    ops.pop();
    double right = values.top(); values.pop();
    double left  = values.top(); values.pop();

    double r = 0.0;
    switch (op) {
        case CalcToken::Kind::Plus:   r = left + right; break;
        case CalcToken::Kind::Minus:  r = left - right; break;
        case CalcToken::Kind::Times:  r = left * right; break;
        case CalcToken::Kind::Divide:
            if (right == 0.0) {
                if (error) *error = "division by zero";
                return false;
            }
            r = left / right;
            break;
        default:
            if (error) *error = "malformed expression";
            return false;
    }
    values.push(r);
    return true;
}

}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out, std::string* error) {
    std::stack<double> values;
    std::stack<CalcToken::Kind> ops;

    for (const CalcToken& t : tokens) {
        switch (t.kind) {
            case CalcToken::Kind::Number:
                values.push(t.value);
                break;
            case CalcToken::Kind::LParen:
                ops.push(t.kind);
                break;
            case CalcToken::Kind::RParen:
                while (!ops.empty() && ops.top() != CalcToken::Kind::LParen)
                    if (!apply_top(values, ops, error)) return false;
                if (ops.empty()) {
                    if (error) *error = "unbalanced parentheses";
                    return false;
                }
                ops.pop();   // discard the '('
                break;
            default:   // + - * /
                // >= (not >) gives left-to-right: 10-4-3 == (10-4)-3
                while (!ops.empty() && is_operator(ops.top()) &&
                       precedence(ops.top()) >= precedence(t.kind))
                    if (!apply_top(values, ops, error)) return false;
                ops.push(t.kind);
                break;
        }
    }

    while (!ops.empty()) {
        if (ops.top() == CalcToken::Kind::LParen) {
            if (error) *error = "unbalanced parentheses";
            return false;
        }
        if (!apply_top(values, ops, error)) return false;
    }

    if (values.size() != 1) {
        if (error) *error = "malformed expression";
        return false;
    }
    *out = values.top();
    return true;
}
