cmake_minimum_required(VERSION 3.25)
if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED MUTATION_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and MUTATION_ROOT are required")
endif()
set(_guard "${SOURCE_ROOT}/tests/platform/full_chinese_inventory_progression_source_test.cmake")
if(NOT EXISTS "${_guard}")
    message(FATAL_ERROR "T6 guard is missing")
endif()
file(READ "${_guard}" _guard_text)
file(REMOVE_RECURSE "${MUTATION_ROOT}")
file(MAKE_DIRECTORY "${MUTATION_ROOT}")
file(COPY "${SOURCE_ROOT}/tests/platform/cpp_source_function_body.cmake"
    DESTINATION "${MUTATION_ROOT}")
set_property(GLOBAL PROPERTY T6_MISSED "")

function(make_tree LABEL OUT)
    set(_root "${MUTATION_ROOT}/${LABEL}")
    file(MAKE_DIRECTORY "${_root}/src/platform/raylib")
    foreach(_file IN ITEMS inventory_view_math.cpp inventory_renderer.cpp ground_loot_view.cpp loot_pickup_feedback.cpp material_loot_view.cpp material_bag_renderer.cpp hud_renderer.cpp passive_tree_view_math.cpp passive_tree_renderer.cpp death_overlay_view.cpp death_overlay_renderer.cpp dungeon_view_math.cpp combat_renderer.cpp ui_text_catalog.cpp ui_text_catalog.hpp ui_text_layout.cpp ui_text_layout.hpp)
        file(COPY "${SOURCE_ROOT}/src/platform/raylib/${_file}" DESTINATION "${_root}/src/platform/raylib")
    endforeach()
    set(${OUT} "${_root}" PARENT_SCOPE)
endfunction()

function(run_guard LABEL ROOT EXPECT_SUCCESS SCRIPT)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${ROOT}" -P "${SCRIPT}"
        RESULT_VARIABLE _result OUTPUT_QUIET ERROR_QUIET)
    if(EXPECT_SUCCESS AND NOT _result EQUAL 0)
        message(FATAL_ERROR "T6 guard unexpectedly rejected ${LABEL}")
    endif()
    if(NOT EXPECT_SUCCESS AND _result EQUAL 0)
        set_property(GLOBAL APPEND PROPERTY T6_MISSED "${LABEL}")
    endif()
endfunction()

function(mutate_and_expect LABEL FILE NEEDLE REPLACEMENT)
    make_tree("${LABEL}" _root)
    set(_path "${_root}/src/platform/raylib/${FILE}")
    file(READ "${_path}" _text)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated "${_text}")
    if(_mutated STREQUAL _text)
        message(FATAL_ERROR "T6 mutation anchor is missing: ${LABEL}")
    endif()
    file(WRITE "${_path}" "${_mutated}")
    run_guard("${LABEL}" "${_root}" FALSE "${_guard}")
endfunction()

function(mutate_and_expect_accept LABEL FILE NEEDLE REPLACEMENT)
    make_tree("${LABEL}" _root)
    set(_path "${_root}/src/platform/raylib/${FILE}")
    file(READ "${_path}" _text)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated "${_text}")
    if(_mutated STREQUAL _text)
        message(FATAL_ERROR "T6 positive mutation anchor is missing: ${LABEL}")
    endif()
    file(WRITE "${_path}" "${_mutated}")
    run_guard("${LABEL}" "${_root}" TRUE "${_guard}")
endfunction()

make_tree(pristine _pristine)
run_guard(pristine "${_pristine}" TRUE "${_guard}")
mutate_and_expect_accept(attribute_control "passive_tree_view_math.cpp"
    "PassiveNodeTextView view{};"
    "[[maybe_unused]] int attribute_control{};\n    PassiveNodeTextView view{};")
mutate_and_expect_accept(spaced_array_control "passive_tree_view_math.cpp"
    "PassiveNodeTextView view{};"
    "PassiveNodeTextView view{};\n    if (view.name [0] == '\\0') { static_cast<void>(view.name [0]); }")
mutate_and_expect(active_english "inventory_renderer.cpp" "ui_item_base_name(item.base_id)" "\"EQUIPMENT INVENTORY\"")
mutate_and_expect(ground_comment_decoy "ground_loot_view.cpp" "ui_item_base_name(item.base_id)" "/* ui_item_base_name(item.base_id) */ base->name.data()")
mutate_and_expect(ground_preprocessor_decoy "ground_loot_view.cpp" "ui_item_base_name(item.base_id)" "#if 0\nui_item_base_name(item.base_id)\n#endif\nbase->name.data()")
mutate_and_expect(ground_lambda_decoy "ground_loot_view.cpp" "ui_item_base_name(item.base_id)" "([&]() { static_cast<void>(ui_item_base_name(item.base_id)); }, u8\"未知\")")
mutate_and_expect(pickup_comment_decoy "loot_pickup_feedback.cpp" "ui_item_base_name(receipt.base_id)" "/* ui_item_base_name(receipt.base_id) */ base->name.data()")
mutate_and_expect(pickup_preprocessor_decoy "loot_pickup_feedback.cpp" "ui_item_base_name(receipt.base_id)" "#if 0\nui_item_base_name(receipt.base_id)\n#endif\nbase->name.data()")
mutate_and_expect(pickup_lambda_decoy "loot_pickup_feedback.cpp" "ui_item_base_name(receipt.base_id)" "([&]() { static_cast<void>(ui_item_base_name(receipt.base_id)); }, u8\"未知\")")
mutate_and_expect(material_comment_decoy "material_loot_view.cpp" "ui_utf8_format(token, sizeof(token)," "/* ui_utf8_format(token, sizeof(token), */ std::snprintf(destination.bytes.data(), 2, \"%s\", u8\"嬗变石\");")
mutate_and_expect(material_preprocessor_decoy "material_loot_view.cpp" "ui_utf8_format(token, sizeof(token)," "#if 0\nui_utf8_format(token, sizeof(token),\n#endif\nstd::snprintf(destination.bytes.data(), 2, \"%s\", u8\"嬗变石\");")
set(_material_format_statement [=[        const UiUtf8CopyResult formatted = ui_utf8_format(token, sizeof(token),
            has_material ? u8"、%s ×%llu" : u8"%s ×%llu",
            ui_material_name(id),
            static_cast<unsigned long long>(counts[index]));]=])
set(_material_lambda_decoy [=[        const auto unused_catalog_format = [&]() {
            static_cast<void>(ui_utf8_format(token, sizeof(token), u8"%s"));
        };
        const UiUtf8CopyResult formatted{};]=])
mutate_and_expect(material_lambda_decoy "material_loot_view.cpp" "${_material_format_statement}" "${_material_lambda_decoy}")
mutate_and_expect(core_base_name "inventory_renderer.cpp" "ui_item_base_name(item.base_id)" "base->name.data()")
mutate_and_expect(core_display_name "inventory_renderer.cpp" "ui_item_base_name(item.base_id)" "base->display_name.data()")
mutate_and_expect(core_passive_source_name "passive_tree_view_math.cpp" "ui_passive_node_name(node)" "source.name.data()")
mutate_and_expect(default_font "death_overlay_renderer.cpp" "draw_text(font_, view.title.data()" "GetFontDefault(); draw_text(font_, view.title.data()")
mutate_and_expect(text_width_backing "inventory_renderer.cpp" "DrawRectangleRec(rectangle, fill)" "DrawRectangleRec({rectangle.x, rectangle.y, MeasureTextEx(font, label, 18.0F, 0.5F).x, rectangle.height}, fill)")
mutate_and_expect(non_centered_button "inventory_renderer.cpp" "draw_centered_hud_font_text(" "draw_hud_font_text(")
mutate_and_expect(transparent_backing "material_bag_renderer.cpp" "opaque_backing.a != 255U" "opaque_backing.a != 254U")
mutate_and_expect(low_contrast "material_bag_renderer.cpp" "ui_luma_contrast_ratio(color, opaque_backing) < 4.5F" "ui_luma_contrast_ratio(color, opaque_backing) < 0.1F")
mutate_and_expect(unsafe_utf8_snprintf "material_loot_view.cpp" "const UiUtf8CopyResult formatted = ui_utf8_format(token, sizeof(token)," "std::snprintf(destination.bytes.data(), 2, \"%s\", u8\"嬗变石\");\n        const UiUtf8CopyResult formatted = ui_utf8_format(token, sizeof(token),")
mutate_and_expect(legacy_material "material_loot_view.cpp" "return ui_material_name(id);" "return u8\"蜕变石\";")
mutate_and_expect(ascii_death "death_overlay_renderer.cpp" "build_death_overlay_view(snapshot, continue_binding_label)" "build_death_overlay_ascii_view(snapshot)")

make_tree(dead_decoy _dead_root)
set(_dead_file "${_dead_root}/src/platform/raylib/inventory_renderer.cpp")
file(READ "${_dead_file}" _dead_text)
string(REPLACE "DrawRectangleRec(rectangle, fill);" "if constexpr (false) { DrawRectangleRec(rectangle, fill); }" _dead_mutated "${_dead_text}")
if(_dead_mutated STREQUAL _dead_text)
    message(FATAL_ERROR "T6 dead-decoy mutation anchor is missing")
endif()
file(WRITE "${_dead_file}" "${_dead_mutated}")
run_guard(dead_decoy "${_dead_root}" FALSE "${_guard}")
set(_dead_function [=[function(reject_dead_branch CODE CONTEXT)
    if("${CODE}" MATCHES "#[ \t]*if[ \t]+0([^0-9]|$)"
            OR "${CODE}" MATCHES "if[ \t]*(constexpr[ \t]*)?\\([ \t]*(false|0|0[ \t]*==[ \t]*1|1[ \t]*==[ \t]*0)[ \t]*\\)"
            OR "${CODE}" MATCHES "if[ \t]*\\([^\\)]*(false[ \t]*&&|![ \t]*true|0[ \t]*!=[ \t]*0|1[ \t]*!=[ \t]*1)[^\\)]*\\)")
        message(FATAL_ERROR "T6 ${CONTEXT} required behavior is hidden in a dead branch")
    endif()
endfunction()]=])
string(REPLACE "${_dead_function}" "function(reject_dead_branch CODE CONTEXT)\nendfunction()" _without_dead "${_guard_text}")
if(_without_dead STREQUAL _guard_text)
    message(FATAL_ERROR "T6 dead-check causality anchor is missing")
endif()
set(_no_dead_guard "${MUTATION_ROOT}/guard_without_dead_check.cmake")
file(WRITE "${_no_dead_guard}" "${_without_dead}")
run_guard(dead_decoy_without_dead_check "${_dead_root}" TRUE "${_no_dead_guard}")

make_tree(preprocessor_decoy _preprocessor_root)
set(_preprocessor_file "${_preprocessor_root}/src/platform/raylib/inventory_renderer.cpp")
file(READ "${_preprocessor_file}" _preprocessor_text)
string(REPLACE "DrawRectangleRec(rectangle, fill);" "#if 0\n    DrawRectangleRec(rectangle, fill);\n#endif" _preprocessor_mutated "${_preprocessor_text}")
file(WRITE "${_preprocessor_file}" "${_preprocessor_mutated}")
run_guard(preprocessor_decoy "${_preprocessor_root}" FALSE "${_guard}")
run_guard(preprocessor_without_dead_check "${_preprocessor_root}" TRUE "${_no_dead_guard}")

make_tree(lambda_decoy _lambda_root)
set(_lambda_file "${_lambda_root}/src/platform/raylib/inventory_renderer.cpp")
file(READ "${_lambda_file}" _lambda_text)
string(REPLACE "DrawRectangleRec(rectangle, fill);" "[&]() { DrawRectangleRec(rectangle, fill); };" _lambda_mutated "${_lambda_text}")
file(WRITE "${_lambda_file}" "${_lambda_mutated}")
run_guard(lambda_decoy "${_lambda_root}" FALSE "${_guard}")
set(_lambda_function [=[function(reject_lambda CODE CONTEXT)
    # CODE has already been stripped of comments and string literals.  Scan
    # capture lists instead of relying on a regex: init captures may nest []
    # and lambda declarators may carry mutable/noexcept/trailing return types.
    string(LENGTH "${CODE}" _length)
    set(_index 0)
    while(_index LESS _length)
        string(SUBSTRING "${CODE}" ${_index} 1 _character)
        if(NOT _character STREQUAL "[")
            math(EXPR _index "${_index} + 1")
            continue()
        endif()
        math(EXPR _following_index "${_index} + 1")
        if(_following_index LESS _length)
            string(SUBSTRING "${CODE}" ${_following_index} 1 _following)
            if(_following STREQUAL "[")
                # C++ attributes ([[maybe_unused]]) are not lambdas.
                math(EXPR _index "${_index} + 2")
                continue()
            endif()
        endif()
        if(_index GREATER 0)
            math(EXPR _previous_index "${_index} - 1")
            while(_previous_index GREATER_EQUAL 0)
                string(SUBSTRING "${CODE}" ${_previous_index} 1 _previous)
                if(NOT _previous MATCHES "[ \t\r\n]")
                    break()
                endif()
                math(EXPR _previous_index "${_previous_index} - 1")
            endwhile()
            # An identifier/index before the capture is an array access even
            # with whitespace (view.name [0]); these C++17 expression
            # keywords/operators may legally precede a lambda.
            if(_previous STREQUAL "]" OR _previous STREQUAL ")")
                math(EXPR _index "${_index} + 1")
                continue()
            endif()
            if("${_previous}" MATCHES "^[A-Za-z0-9_]$")
                set(_word_end ${_previous_index})
                while(_previous_index GREATER 0)
                    math(EXPR _word_start "${_previous_index} - 1")
                    string(SUBSTRING "${CODE}" ${_word_start} 1 _word_character)
                    if(NOT _word_character MATCHES "^[A-Za-z0-9_]$")
                        break()
                    endif()
                    set(_previous_index ${_word_start})
                endwhile()
                math(EXPR _word_length "${_word_end} - ${_previous_index} + 1")
                string(SUBSTRING "${CODE}" ${_previous_index} ${_word_length} _previous_word)
                if(NOT _previous_word STREQUAL "return" AND NOT _previous_word STREQUAL "throw"
                        AND NOT _previous_word STREQUAL "not" AND NOT _previous_word STREQUAL "and"
                        AND NOT _previous_word STREQUAL "or" AND NOT _previous_word STREQUAL "not_eq")
                    math(EXPR _index "${_index} + 1")
                    continue()
                endif()
            endif()
        endif()
        set(_capture_depth 1)
        math(EXPR _cursor "${_index} + 1")
        while(_cursor LESS _length AND _capture_depth GREATER 0)
            string(SUBSTRING "${CODE}" ${_cursor} 1 _capture_character)
            if(_capture_character STREQUAL "[")
                math(EXPR _capture_depth "${_capture_depth} + 1")
            elseif(_capture_character STREQUAL "]")
                math(EXPR _capture_depth "${_capture_depth} - 1")
            endif()
            math(EXPR _cursor "${_cursor} + 1")
        endwhile()
        if(NOT _capture_depth EQUAL 0)
            math(EXPR _index "${_index} + 1")
            continue()
        endif()
        while(_cursor LESS _length)
            string(SUBSTRING "${CODE}" ${_cursor} 1 _space)
            if(NOT _space MATCHES "[ \t\r\n]")
                break()
            endif()
            math(EXPR _cursor "${_cursor} + 1")
        endwhile()
        if(_cursor LESS _length)
            string(SUBSTRING "${CODE}" ${_cursor} 1 _next)
            if(_next STREQUAL "(")
                set(_parameter_depth 1)
                math(EXPR _cursor "${_cursor} + 1")
                while(_cursor LESS _length AND _parameter_depth GREATER 0)
                    string(SUBSTRING "${CODE}" ${_cursor} 1 _parameter_character)
                    if(_parameter_character STREQUAL "(")
                        math(EXPR _parameter_depth "${_parameter_depth} + 1")
                    elseif(_parameter_character STREQUAL ")")
                        math(EXPR _parameter_depth "${_parameter_depth} - 1")
                    endif()
                    math(EXPR _cursor "${_cursor} + 1")
                endwhile()
            endif()
        endif()
        # A valid lambda body is the first brace after its optional declarator.
        # We stop at a statement separator so unrelated array expressions never
        # borrow a later block as their body.
        while(_cursor LESS _length)
            string(SUBSTRING "${CODE}" ${_cursor} 1 _suffix_character)
            if(_suffix_character STREQUAL "{")
                message(FATAL_ERROR "T6 ${CONTEXT} required behavior is hidden in a lambda")
            endif()
            if(_suffix_character STREQUAL ";")
                break()
            endif()
            math(EXPR _cursor "${_cursor} + 1")
        endwhile()
        math(EXPR _index "${_index} + 1")
    endwhile()
endfunction()]=])
string(REPLACE "${_lambda_function}" "function(reject_lambda CODE CONTEXT)\nendfunction()" _without_lambda "${_guard_text}")
if(_without_lambda STREQUAL _guard_text)
    message(FATAL_ERROR "T6 lambda-check causality anchor is missing")
endif()
set(_no_lambda_guard "${MUTATION_ROOT}/guard_without_lambda_check.cmake")
file(WRITE "${_no_lambda_guard}" "${_without_lambda}")
run_guard(lambda_without_lambda_check "${_lambda_root}" TRUE "${_no_lambda_guard}")

function(mutate_lambda_and_prove_causality LABEL FILE NEEDLE REPLACEMENT)
    make_tree("${LABEL}" _root)
    set(_path "${_root}/src/platform/raylib/${FILE}")
    file(READ "${_path}" _text)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated "${_text}")
    if(_mutated STREQUAL _text)
        message(FATAL_ERROR "T6 lambda mutation anchor is missing: ${LABEL}")
    endif()
    file(WRITE "${_path}" "${_mutated}")
    run_guard("${LABEL}" "${_root}" FALSE "${_guard}")
    run_guard("${LABEL}_without_lambda_check" "${_root}" TRUE "${_no_lambda_guard}")
endfunction()

# Each replacement keeps the required catalog/format token solely in an
# uncalled, syntactically valid lambda.  The normal path takes a Chinese-safe
# fallback (or an empty formatting result), so only reject_lambda rejects it.
mutate_lambda_and_prove_causality(ground_lambda_causality "ground_loot_view.cpp"
    "ui_item_base_name(item.base_id)"
    "([&]() { static_cast<void>(ui_item_base_name(item.base_id)); }, u8\"未知\")")
mutate_lambda_and_prove_causality(pickup_lambda_causality "loot_pickup_feedback.cpp"
    "ui_item_base_name(receipt.base_id)"
    "([&]() { static_cast<void>(ui_item_base_name(receipt.base_id)); }, u8\"未知\")")
mutate_lambda_and_prove_causality(material_lambda_causality "material_loot_view.cpp"
    "${_material_format_statement}" "${_material_lambda_decoy}")
mutate_lambda_and_prove_causality(passive_lambda_causality "passive_tree_view_math.cpp"
    "ui_passive_node_name(node)"
    "([&]() noexcept -> const char* { return ui_passive_node_name(node); }, ui_text(UiTextId::unknown))")
mutate_lambda_and_prove_causality(death_lambda_causality "death_overlay_view.cpp"
    "copy_text(view.title, u8\"死亡回顾\");"
    "[&, title = view.title[0]]() noexcept { static_cast<void>(title); copy_text(view.title, u8\"死亡回顾\"); };")
mutate_lambda_and_prove_causality(return_lambda_causality "death_overlay_view.cpp"
    "return ui_monster_name(static_cast<combat::MonsterId>(id));"
    "return[&]() noexcept -> const char* { static_cast<void>(ui_monster_name(static_cast<combat::MonsterId>(id))); return u8\"未知\"; }, u8\"未知\";")
mutate_lambda_and_prove_causality(throw_lambda_causality "death_overlay_view.cpp"
    "return ui_monster_name(static_cast<combat::MonsterId>(id));"
    "throw[&]() noexcept -> const char* { static_cast<void>(ui_monster_name(static_cast<combat::MonsterId>(id))); return u8\"未知\"; };")
mutate_lambda_and_prove_causality(not_lambda_causality "death_overlay_view.cpp"
    "return ui_monster_name(static_cast<combat::MonsterId>(id));"
    "not[]() { static_cast<void>(ui_monster_name(static_cast<combat::MonsterId>(0))); };")
mutate_lambda_and_prove_causality(and_lambda_causality "death_overlay_view.cpp"
    "return ui_monster_name(static_cast<combat::MonsterId>(id));"
    "false and []() { static_cast<void>(ui_monster_name(static_cast<combat::MonsterId>(0))); };")
mutate_lambda_and_prove_causality(or_lambda_causality "death_overlay_view.cpp"
    "return ui_monster_name(static_cast<combat::MonsterId>(id));"
    "false or []() { static_cast<void>(ui_monster_name(static_cast<combat::MonsterId>(0))); };")
mutate_lambda_and_prove_causality(not_eq_lambda_causality "death_overlay_view.cpp"
    "return ui_monster_name(static_cast<combat::MonsterId>(id));"
    "nullptr not_eq []() { static_cast<void>(ui_monster_name(static_cast<combat::MonsterId>(0))); };")

# Regression for whitespace before an array subscript.  The prior scanner used
# the immediate previous character (a space) and then borrowed this if-block's
# brace as a lambda body.  The production scanner must accept it; the copied
# old scanner must reject the same source, proving the new backtracking rule.
set(_new_spaced_array_scanner [=[            math(EXPR _previous_index "${_index} - 1")
            while(_previous_index GREATER_EQUAL 0)
                string(SUBSTRING "${CODE}" ${_previous_index} 1 _previous)
                if(NOT _previous MATCHES "[ \t\r\n]")
                    break()
                endif()
                math(EXPR _previous_index "${_previous_index} - 1")
            endwhile()
            # An identifier/index before the capture is an array access even
            # with whitespace (view.name [0]); these C++17 expression
            # keywords/operators may legally precede a lambda.
            if(_previous STREQUAL "]" OR _previous STREQUAL ")")
                math(EXPR _index "${_index} + 1")
                continue()
            endif()
            if("${_previous}" MATCHES "^[A-Za-z0-9_]$")
                set(_word_end ${_previous_index})
                while(_previous_index GREATER 0)
                    math(EXPR _word_start "${_previous_index} - 1")
                    string(SUBSTRING "${CODE}" ${_word_start} 1 _word_character)
                    if(NOT _word_character MATCHES "^[A-Za-z0-9_]$")
                        break()
                    endif()
                    set(_previous_index ${_word_start})
                endwhile()
                math(EXPR _word_length "${_word_end} - ${_previous_index} + 1")
                string(SUBSTRING "${CODE}" ${_previous_index} ${_word_length} _previous_word)
                if(NOT _previous_word STREQUAL "return" AND NOT _previous_word STREQUAL "throw"
                        AND NOT _previous_word STREQUAL "not" AND NOT _previous_word STREQUAL "and"
                        AND NOT _previous_word STREQUAL "or" AND NOT _previous_word STREQUAL "not_eq")
                    math(EXPR _index "${_index} + 1")
                    continue()
                endif()
            endif()]=])
set(_old_spaced_array_scanner [=[            math(EXPR _previous_index "${_index} - 1")
            string(SUBSTRING "${CODE}" ${_previous_index} 1 _previous)
            if("${_previous}" MATCHES "^[A-Za-z0-9_]$"
                    OR _previous STREQUAL "]" OR _previous STREQUAL ")")
                math(EXPR _index "${_index} + 1")
                continue()
            endif()]=])
make_tree(spaced_array_old_scanner _spaced_root)
set(_spaced_file "${_spaced_root}/src/platform/raylib/passive_tree_view_math.cpp")
file(READ "${_spaced_file}" _spaced_text)
string(REPLACE "PassiveNodeTextView view{};"
    "PassiveNodeTextView view{};\n    if (view.name [0] == '\\0') { static_cast<void>(view.name [0]); }"
    _spaced_mutated "${_spaced_text}")
file(WRITE "${_spaced_file}" "${_spaced_mutated}")
run_guard(spaced_array_current_scanner "${_spaced_root}" TRUE "${_guard}")
string(REPLACE "${_new_spaced_array_scanner}" "${_old_spaced_array_scanner}"
    _old_spaced_guard "${_guard_text}")
if(_old_spaced_guard STREQUAL _guard_text)
    message(FATAL_ERROR "T6 old spaced-array scanner anchor is missing")
endif()
set(_old_spaced_guard_path "${MUTATION_ROOT}/guard_with_old_spaced_array_scanner.cmake")
file(WRITE "${_old_spaced_guard_path}" "${_old_spaced_guard}")
run_guard(spaced_array_old_scanner "${_spaced_root}" FALSE "${_old_spaced_guard_path}")

get_property(_missed GLOBAL PROPERTY T6_MISSED)
if(_missed)
    list(JOIN _missed ", " _joined)
    message(FATAL_ERROR "T6 guard missed mutations: ${_joined}")
endif()
message(STATUS "T6 full Chinese inventory/progression source guard self-test accepted")
