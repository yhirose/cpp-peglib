#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

// --- Definition::error_message ---

TEST(DefinitionApiTest, Error_message_custom) {
  parser parser(R"(
    ROOT       <- GREETING
    GREETING   <- 'hello' ' ' NAME
    NAME       <- [a-z]+ { error_message "expected a lowercase name" }
  )");
  ASSERT_TRUE(parser);

  std::string error_msg;
  parser.set_logger([&](size_t, size_t, const std::string &msg,
                        const std::string &) { error_msg = msg; });

  EXPECT_FALSE(parser.parse("hello 123"));
  EXPECT_EQ(error_msg, "expected a lowercase name");
}

TEST(DefinitionApiTest, Error_message_via_definition) {
  parser parser(R"(
    ROOT <- 'hi ' NAME
    NAME <- [a-z]+
  )");
  ASSERT_TRUE(parser);

  parser["NAME"].error_message = "name must be lowercase letters";

  std::string error_msg;
  parser.set_logger([&](size_t, size_t, const std::string &msg,
                        const std::string &) { error_msg = msg; });

  EXPECT_FALSE(parser.parse("hi 123"));
  EXPECT_EQ(error_msg, "name must be lowercase letters");
}

// --- Definition::no_ast_opt ---

TEST(DefinitionApiTest, No_ast_opt_preserves_node) {
  // Without no_ast_opt
  {
    parser pg(R"(
      ROOT  <- ITEM+
      ITEM  <- [a-z]+
    )");
    ASSERT_TRUE(pg);
    pg.enable_ast();

    std::shared_ptr<Ast> ast;
    EXPECT_TRUE(pg.parse("hello", ast));
    auto optimized = pg.optimize_ast(ast);

    bool found_item = false;
    std::function<void(const Ast &)> walk = [&](const Ast &node) {
      if (node.name == "ITEM") found_item = true;
      for (auto &child : node.nodes) {
        walk(*child);
      }
    };
    walk(*optimized);
    // May or may not have ITEM depending on optimization
    (void)found_item;

    // With no_ast_opt on ITEM
    parser pg2(R"(
      ROOT  <- ITEM+
      ITEM  <- [a-z]+
    )");
    ASSERT_TRUE(pg2);
    pg2["ITEM"].no_ast_opt = true;
    pg2.enable_ast();

    std::shared_ptr<Ast> ast2;
    EXPECT_TRUE(pg2.parse("hello", ast2));
    auto optimized2 = pg2.optimize_ast(ast2);

    bool found_item2 = false;
    std::function<void(const Ast &)> walk2 = [&](const Ast &node) {
      if (node.name == "ITEM") found_item2 = true;
      for (auto &child : node.nodes) {
        walk2(*child);
      }
    };
    walk2(*optimized2);
    EXPECT_TRUE(found_item2);
  }
}

// --- Definition::disable_action ---

TEST(DefinitionApiTest, Disable_action_skips_semantic_action) {
  parser parser(R"(
    ROOT <- NUMBER
    NUMBER <- [0-9]+
  )");
  ASSERT_TRUE(parser);

  bool action_called = false;
  parser["NUMBER"] = [&](const SemanticValues &) { action_called = true; };

  EXPECT_TRUE(parser.parse("123"));
  EXPECT_TRUE(action_called);

  // Now disable the action
  action_called = false;
  parser["NUMBER"].disable_action = true;
  EXPECT_TRUE(parser.parse("456"));
  EXPECT_FALSE(action_called);
}

// --- set_logger (lambda version without rule parameter) ---

TEST(DefinitionApiTest, Set_logger_simple_lambda) {
  parser parser(R"(
    ROOT <- 'hello'
  )");
  ASSERT_TRUE(parser);

  size_t error_line = 0;
  size_t error_col = 0;
  std::string error_msg;
  parser.set_logger([&](size_t line, size_t col, const std::string &msg) {
    error_line = line;
    error_col = col;
    error_msg = msg;
  });

  EXPECT_FALSE(parser.parse("world"));
  EXPECT_GT(error_line, 0u);
  EXPECT_GT(error_col, 0u);
  EXPECT_FALSE(error_msg.empty());
}
