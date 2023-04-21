/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  public static Tree perfect_propagation(Tree input) {
    return input;
  }

  public static Tree obscure_propagation(Tree input) {
    return new Tree(); // See model in `model.json` to treat it as obscure.
  }

  public static Tree into_field_then_obscure_propagation(Tree input) {
    Tree output = new Tree();
    output.a = input;
    return obscure_propagation(output);
  }

  public static Tree obscure_propagation_then_into_field(Tree input) {
    Tree tmp = obscure_propagation(input);
    Tree output = new Tree();
    output.a = tmp;
    return output;
  }

  public static void issue_with_into_field_then_obscure_propagation() {
    Tree input = (Tree) Origin.source();
    Tree output = into_field_then_obscure_propagation(input);
    Origin.sink(output.b); // This is an issue.
  }

  public static void no_issue_with_obscure_propagation_then_into_field() {
    Tree input = (Tree) Origin.source();
    Tree output = obscure_propagation_then_into_field(input);
    Origin.sink(output.b); // This is NOT an issue.
  }

  public static Tree into_field_then_perfect_propagation(Tree input) {
    Tree output = new Tree();
    output.a = input;
    return perfect_propagation(output);
  }

  public static Tree perfect_propagation_then_into_field(Tree input) {
    Tree tmp = perfect_propagation(input);
    Tree output = new Tree();
    output.a = tmp;
    return output;
  }

  public static void no_issue_with_into_field_then_perfect_propagation() {
    Tree input = (Tree) Origin.source();
    Tree output = into_field_then_perfect_propagation(input);
    Origin.sink(output.b); // This is NOT an issue.
  }

  public static void no_issue_with_perfect_propagation_then_into_field() {
    Tree input = (Tree) Origin.source();
    Tree output = perfect_propagation_then_into_field(input);
    Origin.sink(output.b); // This is NOT an issue.
  }

  public static Tree approximate_return_access_path(Tree input) {
    Tree output = new Tree();
    output.a = input.a;
    output.b = input.b;
    output.c = input.c;
    output.d = input.d;
    output.e = input.e;
    output.f = input.f;
    output.g = input.g;
    output.h = input.h;
    output.i = input.i;
    output.j = input.j;
    output.k = input.k;
    output.l = input.l;
    // We should infer Arg(0) -> Return.
    return output;
  }

  public static void issue_approximate_return_access_path() {
    Tree input = new Tree();
    input.a = (Tree) Origin.source();
    Tree output = approximate_return_access_path(input);
    Origin.sink(output.a); // This is an issue.
  }

  public static void issue_approximate_return_access_path_common_prefix() {
    Tree input = new Tree();
    input.a.b = (Tree) Origin.source();
    Tree output = approximate_return_access_path(input);
    Origin.sink(output.a); // This is an issue.
  }

  public static void non_issue_approximate_return_access_path_common_prefix() {
    Tree input = new Tree();
    input.b = (Tree) Origin.source();
    Tree output = approximate_return_access_path(input);
    // This is not an issue, but triggers a false positive, which is expected behavior.
    Origin.sink(output.a);
  }

  public static Tree perfect_propagation_with_tree_manipulation(Tree input) {
    Tree tmp = new Tree();
    tmp.a = input;
    return tmp.a;
  }

  public static Tree collapse_one_append_a_b_c(Tree input) {
    Tree output = new Tree();
    output.a.b.c = input;
    return output;
  }

  public static Tree collapse_one(Tree input) {
    Tree tmp = collapse_one_append_a_b_c(input);
    return tmp.a.b.c;
  }

  public static Tree collapse_two_append_a_b(Tree input) {
    Tree output = new Tree();
    output.a.b = input;
    return output;
  }

  public static Tree collapse_two(Tree input) {
    Tree tmp = collapse_two_append_a_b(input);
    return tmp.a.b;
  }

  public static Tree collapse_three_append_a(Tree input) {
    Tree output = new Tree();
    output.a = input;
    return output;
  }

  public static Tree collapse_three(Tree input) {
    Tree tmp = collapse_three_append_a(input);
    return tmp.a;
  }

  public static Tree into_field_then_collapse_two(Tree input) {
    Tree output = new Tree();
    output.a = input;
    return collapse_two(output);
  }

  public static Tree collapse_two_then_into_field(Tree input) {
    Tree tmp = collapse_two(input);
    Tree output = new Tree();
    output.a = tmp;
    return output;
  }

  public static Tree perfect_propagation_then_into_deep_object(Tree input) {
    Tree tmp = perfect_propagation(input);
    Tree output = new Tree();
    output.a.b.c.d.e = tmp;
    return output;
  }

  public static Tree collapse_two_then_into_deep_object(Tree input) {
    Tree tmp = collapse_two(input);
    Tree output = new Tree();
    output.a.b.c.d.e = tmp;
    return output;
  }

  public static Tree combine_collapse_one(Tree input) {
    Tree first_tmp = new Tree();
    first_tmp.a = input;
    Tree second_tmp = collapse_one(first_tmp);
    Tree third_tmp = new Tree();
    third_tmp.b = second_tmp;
    return collapse_one(third_tmp);
  }

  public static Tree combine_collapse_two(Tree input) {
    Tree first_tmp = new Tree();
    first_tmp.a = input;
    Tree second_tmp = collapse_two(first_tmp);
    Tree third_tmp = new Tree();
    third_tmp.b = second_tmp;
    return collapse_two(third_tmp);
  }

  public static Tree combine_collapse_three(Tree input) {
    Tree first_tmp = new Tree();
    first_tmp.a = input;
    Tree second_tmp = collapse_three(first_tmp);
    Tree third_tmp = new Tree();
    third_tmp.b = second_tmp;
    return collapse_three(third_tmp);
  }

  public static Tree combine_collapse_two_and_one(Tree input) {
    Tree first_tmp = new Tree();
    first_tmp.a = input;
    Tree second_tmp = collapse_two(first_tmp);
    Tree third_tmp = new Tree();
    third_tmp.b = second_tmp;
    return collapse_one(third_tmp);
  }

  public static Tree combine_collapse_one_and_two(Tree input) {
    Tree first_tmp = new Tree();
    first_tmp.a = input;
    Tree second_tmp = collapse_one(first_tmp);
    Tree third_tmp = new Tree();
    third_tmp.b = second_tmp;
    return collapse_two(third_tmp);
  }

  public static Tree loop_perfect_propagation(Tree input) {
    for (int i = 0; i < 100; i++) {
      Tree tmp = new Tree();
      tmp.a = input;
      input = perfect_propagation(tmp);
    }
    return input;
  }

  public static Tree loop_collapse_one(Tree input) {
    for (int i = 0; i < 100; i++) {
      Tree tmp = new Tree();
      tmp.a = input;
      input = collapse_one(tmp);
    }
    return input;
  }

  public static Tree loop_collapse_two(Tree input) {
    for (int i = 0; i < 100; i++) {
      Tree tmp = new Tree();
      tmp.a = input;
      input = collapse_two(tmp);
    }
    return input;
  }

  public static Tree join_collapse_test_1(Tree input, boolean b) {
    Tree output = new Tree();
    if (b) {
      output.a = collapse_two(input);
    } else {
      output.a.b = collapse_one(input);
    }
    return output;
  }

  public static Tree join_collapse_test_2(Tree input, boolean b) {
    Tree output = new Tree();
    if (b) {
      output.a = collapse_two(input);
    } else {
      output.a.b = collapse_three(input);
    }
    return output;
  }

  public static Tree join_collapse_test_3(Tree input, boolean b) {
    if (b) {
      return collapse_one(input);
    } else {
      Tree output = new Tree();
      output.a = collapse_two(input);
      return output;
    }
  }

  public static void issue_join_collapse_test_3(boolean b) {
    Tree input = new Tree();
    input.a = (Tree) Origin.source();
    Tree output = join_collapse_test_3(input, b);
    Origin.sink(output.a.b); // This is an issue.
  }

  public static Tree collapse_one_with_input_path(Tree input) {
    return collapse_one(input.a.b);
  }

  public static Tree collapse_one_with_input_path_with_hop(Tree input) {
    return collapse_one_with_input_path(input);
  }

  public static void no_issue_collapse_two_with_input_path() {
    Tree input = new Tree();
    input.a.b.c = (Tree) Origin.source();
    input.a.b.d = new Tree();
    Tree output = collapse_one_with_input_path(input);
    Origin.sink(output.d); // This is NOT an issue.
  }
}
