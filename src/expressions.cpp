template <class Visitor>
auto visit_expression(expression_t* p, Visitor&& visitor) {
    // clang-format off
    switch (p->type) {
        case exp_identifier:  return visitor(static_cast<concrete_expression_t<exp_identifier>*>(p));     break;
        case exp_constant:    return visitor(static_cast<concrete_expression_t<exp_constant>*>(p));       break;
        case exp_array:       return visitor(static_cast<concrete_expression_t<exp_array>*>(p));          break;
        case exp_call:        return visitor(static_cast<concrete_expression_t<exp_call>*>(p));           break;
        case exp_instanceof:  return visitor(static_cast<concrete_expression_t<exp_instanceof>*>(p));     break;
        case exp_subscript:   return visitor(static_cast<concrete_expression_t<exp_subscript>*>(p));      break;
        case exp_dot:         return visitor(static_cast<concrete_expression_t<exp_dot>*>(p));            break;
        case exp_unary_plus:  return visitor(static_cast<concrete_expression_t<exp_unary_plus>*>(p));     break;
        case exp_unary_minus: return visitor(static_cast<concrete_expression_t<exp_unary_minus>*>(p));    break;
        case exp_not:         return visitor(static_cast<concrete_expression_t<exp_not>*>(p));            break;
        case exp_mul:         return visitor(static_cast<concrete_expression_t<exp_mul>*>(p));            break;
        case exp_div:         return visitor(static_cast<concrete_expression_t<exp_div>*>(p));            break;
        case exp_mod:         return visitor(static_cast<concrete_expression_t<exp_mod>*>(p));            break;
        case exp_add:         return visitor(static_cast<concrete_expression_t<exp_add>*>(p));            break;
        case exp_sub:         return visitor(static_cast<concrete_expression_t<exp_sub>*>(p));            break;
        case exp_lt:          return visitor(static_cast<concrete_expression_t<exp_lt>*>(p));             break;
        case exp_lte:         return visitor(static_cast<concrete_expression_t<exp_lte>*>(p));            break;
        case exp_gt:          return visitor(static_cast<concrete_expression_t<exp_gt>*>(p));             break;
        case exp_gte:         return visitor(static_cast<concrete_expression_t<exp_gte>*>(p));            break;
        case exp_eq:          return visitor(static_cast<concrete_expression_t<exp_eq>*>(p));             break;
        case exp_neq:         return visitor(static_cast<concrete_expression_t<exp_neq>*>(p));            break;
        case exp_and:         return visitor(static_cast<concrete_expression_t<exp_and>*>(p));            break;
        case exp_or:          return visitor(static_cast<concrete_expression_t<exp_or>*>(p));             break;
        case exp_assign:      return visitor(static_cast<concrete_expression_t<exp_assign>*>(p));         break;
        case exp_compile_time_evaluated:
            return visitor(static_cast<concrete_expression_t<exp_compile_time_evaluated>*>(p));           break;
        default: assert(0);   return decltype(visitor(static_cast<concrete_expression_t<exp_or>*>(p)))(); break;
    }
    // clang-format on
}

template <class Visitor>
auto visit_expression(const expression_t* p, Visitor&& visitor) {
    // clang-format off
    switch (p->type) {
        case exp_identifier:  return visitor(static_cast<const concrete_expression_t<exp_identifier>*>(p));     break;
        case exp_constant:    return visitor(static_cast<const concrete_expression_t<exp_constant>*>(p));       break;
        case exp_array:       return visitor(static_cast<const concrete_expression_t<exp_array>*>(p));          break;
        case exp_call:        return visitor(static_cast<const concrete_expression_t<exp_call>*>(p));           break;
        case exp_instanceof:  return visitor(static_cast<const concrete_expression_t<exp_instanceof>*>(p));     break;
        case exp_subscript:   return visitor(static_cast<const concrete_expression_t<exp_subscript>*>(p));      break;
        case exp_dot:         return visitor(static_cast<const concrete_expression_t<exp_dot>*>(p));            break;
        case exp_unary_plus:  return visitor(static_cast<const concrete_expression_t<exp_unary_plus>*>(p));     break;
        case exp_unary_minus: return visitor(static_cast<const concrete_expression_t<exp_unary_minus>*>(p));    break;
        case exp_not:         return visitor(static_cast<const concrete_expression_t<exp_not>*>(p));            break;
        case exp_mul:         return visitor(static_cast<const concrete_expression_t<exp_mul>*>(p));            break;
        case exp_div:         return visitor(static_cast<const concrete_expression_t<exp_div>*>(p));            break;
        case exp_mod:         return visitor(static_cast<const concrete_expression_t<exp_mod>*>(p));            break;
        case exp_add:         return visitor(static_cast<const concrete_expression_t<exp_add>*>(p));            break;
        case exp_sub:         return visitor(static_cast<const concrete_expression_t<exp_sub>*>(p));            break;
        case exp_lt:          return visitor(static_cast<const concrete_expression_t<exp_lt>*>(p));             break;
        case exp_lte:         return visitor(static_cast<const concrete_expression_t<exp_lte>*>(p));            break;
        case exp_gt:          return visitor(static_cast<const concrete_expression_t<exp_gt>*>(p));             break;
        case exp_gte:         return visitor(static_cast<const concrete_expression_t<exp_gte>*>(p));            break;
        case exp_eq:          return visitor(static_cast<const concrete_expression_t<exp_eq>*>(p));             break;
        case exp_neq:         return visitor(static_cast<const concrete_expression_t<exp_neq>*>(p));            break;
        case exp_and:         return visitor(static_cast<const concrete_expression_t<exp_and>*>(p));            break;
        case exp_or:          return visitor(static_cast<const concrete_expression_t<exp_or>*>(p));             break;
        case exp_assign:      return visitor(static_cast<const concrete_expression_t<exp_assign>*>(p));         break;
        case exp_compile_time_evaluated:
              return visitor(static_cast<const concrete_expression_t<exp_compile_time_evaluated>*>(p));         break;
        default: assert(0);   return decltype(visitor(static_cast<const concrete_expression_t<exp_or>*>(p)))(); break;
    }
    // clang-format on
}