function void 
JP_PaintTextLayout(Application_Links *app, Buffer_ID buffer, Text_Layout_ID text_layout_id,
                   Arena *scratch, Token_Array token_array, i64 cursor_pos)
{
    ProfileScope(app, "[Fleury] Render Buffer");
    
    // NOTE(allen): Token colorizing
    if(token_array.tokens != 0)
    {
        F4_SyntaxHighlight(app, text_layout_id, &token_array);
        
        // NOTE(allen): Scan for TODOs and NOTEs
        b32 use_comment_keywords = def_get_config_b32(vars_save_string_lit("use_comment_keywords"));
        if(use_comment_keywords)
        {
            Comment_Highlight_Pair pairs[] =
            {
                {str8_lit("NOTE"), finalize_color(defcolor_comment_pop, 0)},
                {str8_lit("TODO"), finalize_color(defcolor_comment_pop, 1)},
                {def_get_config_string(scratch, vars_save_string_lit("user_name")), finalize_color(fleury_color_comment_user_name, 0)},
            };
            draw_comment_highlights(app, buffer, text_layout_id,
                                    &token_array, pairs, ArrayCount(pairs));
        }
    }
    else
    {
        Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
        paint_text_color_fcolor(app, text_layout_id, visible_range, fcolor_id(defcolor_text_default));
    }
    
    // NOTE(allen): Color parens
    if(def_get_config_b32(vars_save_string_lit("use_paren_helper")))
    {
        Color_Array colors = finalize_color_array(defcolor_text_cycle);
        draw_paren_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
    }
}

//~ NOTE(rjf): Buffer Render
function void
F4_RenderBuffer(Application_Links *app, View_ID view_id, Face_ID face_id,
                Buffer_ID buffer, Text_Layout_ID text_layout_id,
                Rect_f32 rect, Frame_Info frame_info)
{
    Scratch_Block scratch(app);
    ProfileScope(app, "[Fleury] Render Buffer");
    
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);
    Rect_f32 prev_clip = draw_set_clip(app, rect);
    Face_Metrics metrics = get_face_metrics(app, face_id);
    i64 cursor_pos = view_correct_cursor(app, view_id);
    Token_Array token_array = get_token_array_from_buffer(app, buffer);
    
    JP_PaintTextLayout(app, buffer, text_layout_id, scratch, token_array, cursor_pos); 
    
    view_correct_mark(app, view_id);
    
    // NOTE(allen): Scope highlight
    b32 use_scope_highlight = def_get_config_b32(vars_save_string_lit("use_scope_highlight"));
    if (use_scope_highlight){
        Color_Array colors = finalize_color_array(defcolor_back_cycle);
        draw_scope_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
    }
    
    // NOTE(rjf): Brace highlight
    {
        Color_Array colors = finalize_color_array(fleury_color_brace_highlight);
        if(colors.count >= 1 && F4_ARGBIsValid(colors.vals[0]))
        {
            F4_Brace_RenderHighlight(app, buffer, text_layout_id, cursor_pos,
                                     colors.vals, colors.count);
        }
    }
    
    // NOTE(allen): Line highlight
    {
        b32 highlight_line_at_cursor = def_get_config_b32(vars_save_string_lit("highlight_line_at_cursor"));
        String_Const_u8 name = string_u8_litexpr("*compilation*");
        Buffer_ID compilation_buffer = get_buffer_by_name(app, name, Access_Always);
        if(highlight_line_at_cursor && (is_active_view || buffer == compilation_buffer))
        {
            i64 line_number = get_line_number_from_pos(app, buffer, cursor_pos);
            draw_line_highlight(app, text_layout_id, line_number,
                                fcolor_id(defcolor_highlight_cursor_line));
        }
    }
    
    // NOTE(rjf): Error/Search Highlight
    {
        b32 use_error_highlight = def_get_config_b32(vars_save_string_lit("use_error_highlight"));
        b32 use_jump_highlight = def_get_config_b32(vars_save_string_lit("use_jump_highlight"));
        if (use_error_highlight || use_jump_highlight){
            // NOTE(allen): Error highlight
            String_Const_u8 name = string_u8_litexpr("*compilation*");
            Buffer_ID compilation_buffer = get_buffer_by_name(app, name, Access_Always);
            if (use_error_highlight){
                draw_jump_highlights(app, buffer, text_layout_id, compilation_buffer,
                                     fcolor_id(defcolor_highlight_junk));
            }
            
            // NOTE(allen): Search highlight
            if (use_jump_highlight){
                Buffer_ID jump_buffer = get_locked_jump_buffer(app);
                if (jump_buffer != compilation_buffer){
                    draw_jump_highlights(app, buffer, text_layout_id, jump_buffer,
                                         fcolor_id(defcolor_highlight_white));
                }
            }
        }
    }
    
    // NOTE(rjf): Error annotations
    {
        String_Const_u8 name = string_u8_litexpr("*compilation*");
        Buffer_ID compilation_buffer = get_buffer_by_name(app, name, Access_Always);
        F4_RenderErrorAnnotations(app, buffer, text_layout_id, compilation_buffer);
    }
    
    // NOTE(jack): Token Occurance Highlight
    if (!def_get_config_b32(vars_save_string_lit("f4_disable_cursor_token_occurance")))
    {
        ProfileScope(app, "[Fleury] Token Occurance Highlight");
        
        // NOTE(jack): Get the active cursor's token string
        Buffer_ID active_cursor_buffer = view_get_buffer(app, active_view, Access_Always);
        i64 active_cursor_pos = view_get_cursor_pos(app, active_view);
        Token_Array active_cursor_buffer_tokens = get_token_array_from_buffer(app, active_cursor_buffer);
        Token_Iterator_Array active_cursor_it = token_iterator_pos(0, &active_cursor_buffer_tokens, active_cursor_pos);
        Token *active_cursor_token = token_it_read(&active_cursor_it);
        
        String_Const_u8 active_cursor_string = string_u8_litexpr("");
        if(active_cursor_token)
        {
            active_cursor_string = push_buffer_range(app, scratch, active_cursor_buffer, Ii64(active_cursor_token));
            
            // Loop the visible tokens
            Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
            i64 first_index = token_index_from_pos(&token_array, visible_range.first);
            Token_Iterator_Array it = token_iterator_index(0, &token_array, first_index);
            for (;;)
            {
                Token *token = token_it_read(&it);
                if(!token || token->pos >= visible_range.one_past_last)
                {
                    break;
                }
                
                if (token->kind == TokenBaseKind_Identifier)
                {
                    Range_i64 token_range = Ii64(token);
                    String_Const_u8 token_string = push_buffer_range(app, scratch, buffer, token_range);
                    
                    // NOTE(jack) If this is the buffers cursor token, highlight it with an Underline
                    if (range_contains(token_range, view_get_cursor_pos(app, view_id)))
                    {
                        F4_RenderRangeHighlight(app, view_id, text_layout_id,
                                                token_range, F4_RangeHighlightKind_Underline,
                                                fcolor_resolve(fcolor_id(fleury_color_token_highlight)));
                    }
                    // NOTE(jack): If the token matches the active buffer token. highlight it with a Minor Underline
                    else if(active_cursor_token->kind == TokenBaseKind_Identifier && 
                            string_match(token_string, active_cursor_string))
                    {
                        F4_RenderRangeHighlight(app, view_id, text_layout_id,
                                                token_range, F4_RangeHighlightKind_MinorUnderline,
                                                fcolor_resolve(fcolor_id(fleury_color_token_minor_highlight)));
                        
                    } 
                }
                
                if(!token_it_inc_non_whitespace(&it))
                {
                    break;
                }
            }
        }
    }
    else
    {
        // NOTE(jack): if "f4_disable_cursor_token_occurance" is set, just highlight the cusror 
        ProfileScope(app, "[Fleury] Token Highlight");
        
        Token_Iterator_Array it = token_iterator_pos(0, &token_array, cursor_pos);
        Token *token = token_it_read(&it);
        if(token && token->kind == TokenBaseKind_Identifier)
        {
            F4_RenderRangeHighlight(app, view_id, text_layout_id,
                                    Ii64(token->pos, token->pos + token->size),
                                    F4_RangeHighlightKind_Underline,
                                    fcolor_resolve(fcolor_id(fleury_color_token_highlight)));
        }
    }
    
    // NOTE(rjf): Flashes
    {
        F4_RenderFlashes(app, view_id, text_layout_id);
    }
    
    // NOTE(rjf): Divider Comments
    {
        F4_RenderDividerComments(app, buffer, view_id, text_layout_id);
    }
    
    // NOTE(rjf): Cursor Mark Range
    if(is_active_view && fcoder_mode == FCoderMode_Original)
    {
        F4_HighlightCursorMarkRange(app, view_id);
    }
    
    // NOTE(allen): Cursor shape
    u64 cursor_roundness_100 = def_get_config_u64(app, vars_save_string_lit("cursor_roundness"));
    f32 cursor_roundness = metrics.normal_advance*cursor_roundness_100*0.01f;
    f32 mark_thickness = (f32)def_get_config_u64(app, vars_save_string_lit("mark_thickness"));
    
    // NOTE(rjf): Brace annotations
    {
        F4_Brace_RenderCloseBraceAnnotation(app, buffer, text_layout_id, cursor_pos);
    }
    
    // NOTE(rjf): Brace lines
    {
        F4_Brace_RenderLines(app, buffer, view_id, text_layout_id, cursor_pos);
    }
    
    F4_Cursor_Render(app, view_id, is_active_view, buffer, text_layout_id, cursor_roundness, mark_thickness, frame_info);
    // NOTE(allen): put the actual text on the actual screen
    draw_text_layout_default(app, text_layout_id);
    
    // NOTE(rjf): Draw tooltips and stuff.
    if(active_view == view_id)
    {
        // NOTE(rjf): Position context helper
        {
            F4_PosContext_Render(app, view_id, buffer, text_layout_id, cursor_pos);
        }
        
        // NOTE(rjf): Draw tooltip list.
        {
            Mouse_State mouse = get_mouse_state(app);
            
            Rect_f32 view_rect = view_get_screen_rect(app, view_id);
            
            Face_ID tooltip_face_id = global_small_code_face;
            Face_Metrics tooltip_face_metrics = get_face_metrics(app, tooltip_face_id);
            
            Rect_f32 tooltip_rect =
            {
                (f32)mouse.x + 16,
                (f32)mouse.y + 16,
                (f32)mouse.x + 16,
                (f32)mouse.y + 16 + tooltip_face_metrics.line_height + 8,
            };
            
            for(int i = 0; i < global_tooltip_count; ++i)
            {
                String_Const_u8 string = global_tooltips[i].string;
                tooltip_rect.x1 = tooltip_rect.x0;
                tooltip_rect.x1 += get_string_advance(app, tooltip_face_id, string) + 4;
                
                if(tooltip_rect.x1 > view_rect.x1)
                {
                    f32 difference = tooltip_rect.x1 - view_rect.x1;
                    tooltip_rect.x1 = (float)(int)(tooltip_rect.x1 - difference);
                    tooltip_rect.x0 = (float)(int)(tooltip_rect.x0 - difference);
                }
                
                F4_DrawTooltipRect(app, tooltip_rect);
                
                draw_string(app, tooltip_face_id, string,
                            V2f32(tooltip_rect.x0 + 4,
                                  tooltip_rect.y0 + 4),
                            global_tooltips[i].color);
            }
        }
    }
    
    // NOTE(rjf): Draw inactive rectangle
    if(is_active_view == 0)
    {
        Rect_f32 view_rect = view_get_screen_rect(app, view_id);
        ARGB_Color color = fcolor_resolve(fcolor_id(fleury_color_inactive_pane_overlay));
        if(F4_ARGBIsValid(color))
        {
            draw_rectangle(app, view_rect, 0.f, color);
        }
    }
    
    // NOTE(rjf): Render code peek.
    {
        if(!view_get_is_passive(app, view_id) &&
           !is_active_view)
        {
            F4_CodePeek_Render(app, view_id, face_id, buffer, frame_info);
        }
    }
    
    
    //~ NOTE(jack): Draw @Autocompletion suggestion
    {
        // TODO(jack): 
        // [ ] Better manage the animation bits
        // [ ] Don't suggest on numeric tokens?
        // [ ] Some kind of motion timeout for inline helpers so that you have to 
        //     wait a little before they show up, saves sporadic movements when code navigating
        //      - I may want this on line trailing helpers as well? Experimnet.
        // [ ] Figure out the weird wrapping behaviour if I dont disable line wrapping for the
        //     buffer before drawing the relocated text. It's strage to see wrapped content
        //     reappering on the cursor line when a helper shows up (But not hte end of the world).
        if (is_active_view)
        {
            // NOTE(jack): Animation Statics:
            static f32 cur_x_off = 0.0f;
            static f32 next_x_off = 0.0f;
            static float midline_helper_delay = 1.0f;
            static float midline_helper_current_t = 0.0f;
            
            i64 line = get_line_number_from_pos(app, buffer, cursor_pos);
            i64 line_end = get_line_side_pos(app, buffer, line, Side_Max);
            i64 line_start = get_line_side_pos(app, buffer, line, Side_Min);
            String_Const_u8 trailing_string = push_buffer_range(app, scratch, buffer, {cursor_pos, line_end});
            
            ARGB_Color color = finalize_color(defcolor_text_default, 0);
            // Set Alpha channel to 0x80
            color &= 0x00FFFFFF;
            color |= 0x40000000;
            
            b32 is_last_non_whitespace = (string_skip_chop_whitespace(trailing_string).size == 0);
            
            i64 token_end = scan(app, boundary_alpha_numeric_underscore_utf8, buffer, Scan_Forward, cursor_pos-1);
            if (cursor_pos == token_end)
            {
                // TODO(jack): Actually verify this works, when I was using the current scratch block for the iterator 
                // arena it would crash by access violation to pointer which was seamingly in a freed block of the arena.
                // Couple days later of using it, and I'm yet to see it crash so i _think_ we're good.
                Arena arena = make_arena_system();
                Rect_f32 cursor_rect = text_layout_character_on_screen(app, text_layout_id, cursor_pos);
                
                Word_Complete_Iterator word_complete_iter = {};
                word_complete_iter.app = app;
                word_complete_iter.arena = &arena;
                Word_Complete_Iterator *it = &word_complete_iter;
                
                Range_i64 range = get_word_complete_needle_range(app, buffer, cursor_pos);
                word_complete_iter_init(buffer, range, it);
                // NOTE(jack): If we dont iterate once, the read will return the "needle"
                word_complete_iter_next(it);
                
                String_Const_u8 completion = word_complete_iter_read(it);
                String_Const_u8 remaining_completion = string_skip(completion, range_size(range));
                
                if (remaining_completion.size > 0)
                {
                    if (is_last_non_whitespace)
                    {
                        // NOTE(jack): We can safely just draw after the cursor immediatley.
                        Vec2_f32 draw_start = cursor_rect.p0;
                        draw_string(app, face_id, remaining_completion, draw_start, color);
                    }
                    else
                    {
                        // NOTE(jack): Wait 3 seconds before showing the mid-line helper
                        if (midline_helper_current_t < midline_helper_delay)
                        {
                            midline_helper_current_t += frame_info.animation_dt;
                            animate_in_n_milliseconds(app, 0);
                        }
                        else
                        {
                            Vec2_f32 draw_start = cursor_rect.p0;
                            Rect_f32 screen_rect = view_get_screen_rect(app, view_id);
                            
                            f32 remaining_x_offset = metrics.normal_advance * (cursor_pos - line_start);
                            Buffer_Point bp;
                            bp.line_number = line;
                            bp.pixel_shift = V2f32(remaining_x_offset, 0.0f);
                            
                            Rect_f32 line_rect = Rf32(draw_start.x, draw_start.y,
                                                      screen_rect.x1, draw_start.y + metrics.line_height);
                            
                            // TODO(jack): Animate here
                            // cur += (nxt - cur) * (1.f - Pow(rate, app.dt))
                            next_x_off = metrics.normal_advance * remaining_completion.size;
                            cur_x_off += (next_x_off - cur_x_off) * (1.0f - powf(1e-11f, frame_info.animation_dt));
                            
                            if (fabsf(next_x_off - cur_x_off) > 0.0001f) {
                                animate_in_n_milliseconds(app, 0);
                            }
                            
                            Rect_f32 layout_rect = line_rect;
                            layout_rect.x0 = draw_start.x + cur_x_off;
                            //layout_rect.x0 += metrics.normal_advance * remaining_completion.size;
                            
                            // NOTE(jack): Disable  
                            b32 old_wrap_lines = false;
                            Managed_Scope scope = buffer_get_managed_scope(app, buffer);
                            b32 *wrap_lines_ptr = scope_attachment(app, scope, buffer_wrap_lines, b32);
                            if (wrap_lines_ptr != 0){
                                old_wrap_lines = *wrap_lines_ptr;
                                *wrap_lines_ptr = false;
                                buffer_clear_layout_cache(app, buffer);
                            }
                            
                            Text_Layout_ID remaining_layout_layout = text_layout_create(app, buffer, layout_rect, bp);
                            
                            JP_PaintTextLayout(app, buffer, remaining_layout_layout, scratch, token_array, cursor_pos); 
                            
                            draw_rectangle_fcolor(app, line_rect, 0, fcolor_id(defcolor_back));
                            draw_rectangle_fcolor(app, line_rect, 0, fcolor_id(defcolor_highlight_cursor_line));
                            draw_string(app, face_id, remaining_completion, draw_start, color);
                            
                            // draw_rectangle_outline(app, layout_rect, 4.0f, 2.0f, 0xff00FF00);
                            
                            // NOTE(jack) If we dont set the clip rect the whole line will get rendered
                            // even if it is outside of the layout_rect 
                            Rect_f32 old = draw_set_clip(app, layout_rect);
                            draw_text_layout_default(app, remaining_layout_layout);
                            old = draw_set_clip(app, old);
                            
                            if (wrap_lines_ptr != 0) {
                                *wrap_lines_ptr = old_wrap_lines;
                                buffer_clear_layout_cache(app, buffer);
                            }
                            
                            // NOTE(jack): Redraw the cursor as the background rect coveres it.
                            F4_Cursor_Render(app, view_id, is_active_view, buffer, text_layout_id, cursor_roundness, mark_thickness, frame_info);
                        }
                    }
                }
                else 
                {
                    // TODO(jack): Find a better place to reset this?
                    // Having to do it in two places feels wrong 
                    // (tbh i probably want to completely rewrite this to be a little less messy)
                    cur_x_off = 0.0f;
                    midline_helper_current_t = 0.0f;
                }
                
                linalloc_clear(&arena);
            }
            else
            {
                midline_helper_current_t = 0.0f;
                cur_x_off = 0.0f;
            }
        }
    }
    draw_set_clip(app, prev_clip);
}

//~ NOTE(rjf): Render hook
function void
F4_DrawFileBar(Application_Links *app, View_ID view_id, Buffer_ID buffer, Face_ID face_id, Rect_f32 bar)
{
    Scratch_Block scratch(app);
    
    draw_rectangle_fcolor(app, bar, 0.f, fcolor_id(defcolor_bar));
    
    FColor base_color = fcolor_id(defcolor_base);
    FColor pop2_color = fcolor_id(defcolor_pop2);
    
    i64 cursor_position = view_get_cursor_pos(app, view_id);
    Buffer_Cursor cursor = view_compute_cursor(app, view_id, seek_pos(cursor_position));
    
    Fancy_Line list = {};
    String_Const_u8 unique_name = push_buffer_unique_name(app, scratch, buffer);
    push_fancy_string(scratch, &list, base_color, unique_name);
    push_fancy_stringf(scratch, &list, base_color, " - Row: %3.lld Col: %3.lld -", cursor.line, cursor.col);
    
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    Line_Ending_Kind *eol_setting = scope_attachment(app, scope, buffer_eol_setting,
                                                     Line_Ending_Kind);
    switch (*eol_setting){
        case LineEndingKind_Binary:
        {
            push_fancy_string(scratch, &list, base_color, string_u8_litexpr(" bin"));
        }break;
        
        case LineEndingKind_LF:
        {
            push_fancy_string(scratch, &list, base_color, string_u8_litexpr(" lf"));
        }break;
        
        case LineEndingKind_CRLF:
        {
            push_fancy_string(scratch, &list, base_color, string_u8_litexpr(" crlf"));
        }break;
    }
    
    u8 space[3];
    {
        Dirty_State dirty = buffer_get_dirty_state(app, buffer);
        String_u8 str = Su8(space, 0, 3);
        if (dirty != 0){
            string_append(&str, string_u8_litexpr(" "));
        }
        if (HasFlag(dirty, DirtyState_UnsavedChanges)){
            string_append(&str, string_u8_litexpr("*"));
        }
        if (HasFlag(dirty, DirtyState_UnloadedChanges)){
            string_append(&str, string_u8_litexpr("!"));
        }
        push_fancy_string(scratch, &list, pop2_color, str.string);
    }
    
    push_fancy_string(scratch, &list, base_color, S8Lit(" Syntax Mode: "));
    push_fancy_string(scratch, &list, base_color, F4_SyntaxOptionString());
    
    Vec2_f32 p = bar.p0 + V2f32(2.f, 2.f);
    draw_fancy_line(app, face_id, fcolor_zero(), &list, p);
    
    if(!def_get_config_b32(vars_save_string_lit("f4_disable_progress_bar")))
    {
        f32 progress = (f32)cursor.line / (f32)buffer_get_line_count(app, buffer);
        Rect_f32 progress_bar_rect =
        {
            bar.x0 + (bar.x1 - bar.x0) * progress,
            bar.y0,
            bar.x1,
            bar.y1,
        };
        ARGB_Color progress_bar_color = fcolor_resolve(fcolor_id(fleury_color_file_progress_bar));
        if(F4_ARGBIsValid(progress_bar_color))
        {
            draw_rectangle(app, progress_bar_rect, 0, progress_bar_color);
        }
    }
}

function void
F4_Render(Application_Links *app, Frame_Info frame_info, View_ID view_id)
{
    F4_RecentFiles_RefreshView(app, view_id);
    
    ProfileScope(app, "[Fleury] Render");
    Scratch_Block scratch(app);
    
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);
    
    Rect_f32 view_rect = view_get_screen_rect(app, view_id);
    
    Buffer_ID buffer = view_get_buffer(app, view_id, Access_Always);
    String_Const_u8 buffer_name = push_buffer_base_name(app, scratch, buffer);
    
    //~ NOTE(rjf): Draw background.
    {
        ARGB_Color color = fcolor_resolve(fcolor_id(defcolor_back));
        if(string_match(buffer_name, string_u8_litexpr("*compilation*")))
        {
            color = color_blend(color, 0.5f, 0xff000000);
        }
        // NOTE(rjf): Inactive background color.
        else if(is_active_view == 0)
        {
            ARGB_Color inactive_bg_color = fcolor_resolve(fcolor_id(fleury_color_inactive_pane_background));
            if(F4_ARGBIsValid(inactive_bg_color))
            {
                color = inactive_bg_color;
            }
        }
        draw_rectangle(app, view_rect, 0.f, color);
    }
    
    Rect_f32 region = draw_background_and_margin(app, view_id, is_active_view);
    Rect_f32 prev_clip = draw_set_clip(app, region);
    
    Face_ID face_id = get_face_id(app, buffer);
    Face_Metrics face_metrics = get_face_metrics(app, face_id);
    f32 line_height = face_metrics.line_height;
    f32 digit_advance = face_metrics.decimal_digit_advance;
    
    // NOTE(allen): file bar
    b64 showing_file_bar = false;
    if(view_get_setting(app, view_id, ViewSetting_ShowFileBar, &showing_file_bar) && showing_file_bar)
    {
        Rect_f32_Pair pair = layout_file_bar_on_top(region, line_height);
        F4_DrawFileBar(app, view_id, buffer, face_id, pair.min);
        region = pair.max;
    }
    
    Buffer_Scroll scroll = view_get_buffer_scroll(app, view_id);
    Buffer_Point_Delta_Result delta = delta_apply(app, view_id, frame_info.animation_dt, scroll);
    
    if(!block_match_struct(&scroll.position, &delta.point))
    {
        block_copy_struct(&scroll.position, &delta.point);
        view_set_buffer_scroll(app, view_id, scroll, SetBufferScroll_NoCursorChange);
    }
    
    if(delta.still_animating)
    {
        animate_in_n_milliseconds(app, 0);
    }
    
    // NOTE(allen): query bars
    {
        Query_Bar *space[32];
        Query_Bar_Ptr_Array query_bars = {};
        query_bars.ptrs = space;
        if (get_active_query_bars(app, view_id, ArrayCount(space), &query_bars))
        {
            for (i32 i = 0; i < query_bars.count; i += 1)
            {
                Rect_f32_Pair pair = layout_query_bar_on_top(region, line_height, 1);
                draw_query_bar(app, query_bars.ptrs[i], face_id, pair.min);
                region = pair.max;
            }
        }
    }
    
    // NOTE(allen): FPS hud
    if(show_fps_hud)
    {
        Rect_f32_Pair pair = layout_fps_hud_on_bottom(region, line_height);
        draw_fps_hud(app, frame_info, face_id, pair.max);
        region = pair.min;
        animate_in_n_milliseconds(app, 1000);
    }
    
    // NOTE(allen): layout line numbers
    Rect_f32 line_number_rect = {};
    if(def_get_config_b32(vars_save_string_lit("show_line_number_margins")))
    {
        Rect_f32_Pair pair = layout_line_number_margin(app, buffer, region, digit_advance);
        line_number_rect = pair.min;
        line_number_rect.x1 += 4;
        region = pair.max;
    }
    
    // NOTE(allen): begin buffer render
    Buffer_Point buffer_point = scroll.position;
    Text_Layout_ID text_layout_id = text_layout_create(app, buffer, region, buffer_point);
    
    // NOTE(allen): draw line numbers
    if(def_get_config_b32(vars_save_string_lit("show_line_number_margins")))
    {
        draw_line_number_margin(app, view_id, buffer, face_id, text_layout_id, line_number_rect);
    }
    
    // NOTE(allen): draw the buffer
    F4_RenderBuffer(app, view_id, face_id, buffer, text_layout_id, region, frame_info);
    
    text_layout_free(app, text_layout_id);
    draw_set_clip(app, prev_clip);
}

//~ NOTE(rjf): Begin buffer hook

function void
F4_DoFullLex_ASYNC_Inner(Async_Context *actx, Buffer_ID buffer_id)
{
    Application_Links *app = actx->app;
    ProfileScope(app, "[F4] Async Lex");
    Scratch_Block scratch(app);
    
    String_Const_u8 contents = {};
    {
        ProfileBlock(app, "[F4] Async Lex Contents (before mutex)");
        acquire_global_frame_mutex(app);
        ProfileBlock(app, "[F4] Async Lex Contents (after mutex)");
        contents = push_whole_buffer(app, scratch, buffer_id);
        release_global_frame_mutex(app);
    }
    
    i32 limit_factor = 10000;
    
    Token_List list = {};
    b32 canceled = false;
    
    F4_Language *language = F4_LanguageFromBuffer(app, buffer_id);
    
    // NOTE(rjf): Fall back to C++ if we don't have a proper language.
    if(language == 0)
    {
        language = F4_LanguageFromString(S8Lit("cpp"));
    }
    
    if(language != 0)
    {
        void *lexing_state = push_array_zero(scratch, u8, language->lex_state_size);
        language->LexInit(lexing_state, contents);
        for(;;)
        {
            ProfileBlock(app, "[F4] Async Lex Block");
            if(language->LexFullInput(scratch, &list, lexing_state, limit_factor))
            {
                break;
            }
            if(async_check_canceled(actx))
            {
                canceled = true;
                break;
            }
        }
    }
    
    if(!canceled)
    {
        ProfileBlock(app, "[F4] Async Lex Save Results (before mutex)");
        acquire_global_frame_mutex(app);
        ProfileBlock(app, "[F4] Async Lex Save Results (after mutex)");
        Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
        if(scope != 0)
        {
            Base_Allocator *allocator = managed_scope_allocator(app, scope);
            Token_Array *tokens_ptr = scope_attachment(app, scope, attachment_tokens, Token_Array);
            base_free(allocator, tokens_ptr->tokens);
            Token_Array tokens = {};
            tokens.tokens = base_array(allocator, Token, list.total_count);
            tokens.count = list.total_count;
            tokens.max = list.total_count;
            token_fill_memory_from_list(tokens.tokens, &list);
            block_copy_struct(tokens_ptr, &tokens);
        }
        buffer_mark_as_modified(buffer_id);
        release_global_frame_mutex(app);
    }
}

function void
F4_DoFullLex_ASYNC(Async_Context *actx, String_Const_u8 data)
{
    if(data.size == sizeof(Buffer_ID))
    {
        Buffer_ID buffer = *(Buffer_ID*)data.str;
        F4_DoFullLex_ASYNC_Inner(actx, buffer);
    }
}

function BUFFER_HOOK_SIG(F4_BeginBuffer)
{
    ProfileScope(app, "[Fleury] Begin Buffer");
    
    Scratch_Block scratch(app);
    b32 treat_as_code = false;
    String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer_id);
    String_Const_u8 buffer_name = push_buffer_base_name(app, scratch, buffer_id);
    
    // NOTE(rjf): Treat as code if the config tells us to.
    if(treat_as_code == false)
    {
        if(file_name.size > 0)
        {
            String_Const_u8 treat_as_code_string = def_get_config_string(scratch, vars_save_string_lit("treat_as_code"));
            String_Const_u8_Array extensions = parse_extension_line_to_extension_list(app, scratch, treat_as_code_string);
            String_Const_u8 ext = string_file_extension(file_name);
            for(i32 i = 0; i < extensions.count; ++i)
            {
                if(string_match(ext, extensions.strings[i]))
                {
                    treat_as_code = true;
                    break;
                }
            }
        }
    }
    
    // NOTE(rjf): Treat as code for *calc* and *peek* buffers.
    if(treat_as_code == false)
    {
        if(buffer_name.size > 0)
        {
            if(string_match(buffer_name, string_u8_litexpr("*calc*")))
            {
                treat_as_code = true;
            }
            else if(string_match(buffer_name, string_u8_litexpr("*peek*")))
            {
                treat_as_code = true;
            }
        }
    }
    
    // NOTE(rjf): Treat as code if we've identified the language of a file.
    if(treat_as_code == false)
    {
        F4_Language *language = F4_LanguageFromBuffer(app, buffer_id);
        if(language)
        {
            treat_as_code = true;
        }
    }
    
    String_ID file_map_id = vars_save_string_lit("keys_file");
    String_ID code_map_id = vars_save_string_lit("keys_code");
    
    Command_Map_ID map_id = (treat_as_code) ? (code_map_id) : (file_map_id);
    Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
    Command_Map_ID *map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);
    *map_id_ptr = map_id;
    
    Line_Ending_Kind setting = guess_line_ending_kind_from_buffer(app, buffer_id);
    Line_Ending_Kind *eol_setting = scope_attachment(app, scope, buffer_eol_setting, Line_Ending_Kind);
    *eol_setting = setting;
    
    // NOTE(allen): Decide buffer settings
    b32 wrap_lines = true;
    b32 use_lexer = false;
    if(treat_as_code)
    {
        wrap_lines = def_get_config_b32(vars_save_string_lit("enable_code_wrapping"));
        use_lexer = true;
    }
    
    /*
    if(string_match(buffer_name, string_u8_litexpr("*compilation*")))
    {
        wrap_lines = false;
    }
    */
    if (use_lexer)
    {
        ProfileBlock(app, "begin buffer kick off lexer");
        Async_Task *lex_task_ptr = scope_attachment(app, scope, buffer_lex_task, Async_Task);
        *lex_task_ptr = async_task_no_dep(&global_async_system, F4_DoFullLex_ASYNC, make_data_struct(&buffer_id));
    }
    
    {
        b32 *wrap_lines_ptr = scope_attachment(app, scope, buffer_wrap_lines, b32);
        *wrap_lines_ptr = wrap_lines;
    }
    
    if (use_lexer)
    {
        buffer_set_layout(app, buffer_id, layout_virt_indent_index_generic);
    }
    else
    {
        if (treat_as_code)
        {
            buffer_set_layout(app, buffer_id, layout_virt_indent_literal_generic);
        }
        else{
            buffer_set_layout(app, buffer_id, layout_generic);
        }
    }
    
    
    // no meaning for return
    return(0);
}


//~ NOTE(rjf): Layout

function Layout_Item_List
F4_LayoutInner(Application_Links *app, Arena *arena, Buffer_ID buffer, Range_i64 range, Face_ID face, f32 width, Layout_Virtual_Indent virt_indent)
{
    Layout_Item_List list = get_empty_item_list(range);
    
    Scratch_Block scratch(app);
    String_Const_u8 text = push_buffer_range(app, scratch, buffer, range);
    
    Face_Advance_Map advance_map = get_face_advance_map(app, face);
    Face_Metrics metrics = get_face_metrics(app, face);
    f32 tab_width = (f32)def_get_config_u64(app, vars_save_string_lit("default_tab_width"));
    tab_width = clamp_bot(1, tab_width);
    LefRig_TopBot_Layout_Vars pos_vars = get_lr_tb_layout_vars(&advance_map, &metrics, tab_width, width);
    
    if (text.size == 0){
        lr_tb_write_blank(&pos_vars, face, arena, &list, range.first);
    }
    else{
        b32 skipping_leading_whitespace = (virt_indent == LayoutVirtualIndent_On);
        Newline_Layout_Vars newline_vars = get_newline_layout_vars();
        
        u8 *ptr = text.str;
        u8 *end_ptr = ptr + text.size;
        for (;ptr < end_ptr;){
            Character_Consume_Result consume = utf8_consume(ptr, (u64)(end_ptr - ptr));
            
            i64 index = layout_index_from_ptr(ptr, text.str, range.first);
            switch (consume.codepoint){
                case '\t':
                case ' ':
                {
                    newline_layout_consume_default(&newline_vars);
                    f32 advance = lr_tb_advance(&pos_vars, face, consume.codepoint);
                    if (!skipping_leading_whitespace){
                        lr_tb_write_with_advance(&pos_vars, face, advance, arena, &list, index, consume.codepoint);
                    }
                    else{
                        lr_tb_advance_x_without_item(&pos_vars, advance);
                    }
                }break;
                
                default:
                {
                    newline_layout_consume_default(&newline_vars);
                    lr_tb_write(&pos_vars, face, arena, &list, index, consume.codepoint);
                }break;
                
                case '\r':
                {
                    newline_layout_consume_CR(&newline_vars, index);
                }break;
                
                case '\n':
                {
                    i64 newline_index = newline_layout_consume_LF(&newline_vars, index);
                    lr_tb_write_blank(&pos_vars, face, arena, &list, newline_index);
                    lr_tb_next_line(&pos_vars);
                }break;
                
                case max_u32:
                {
                    newline_layout_consume_default(&newline_vars);
                    lr_tb_write_byte(&pos_vars, face, arena, &list, index, *ptr);
                }break;
            }
            
            ptr += consume.inc;
        }
        
        if (newline_layout_consume_finish(&newline_vars)){
            i64 index = layout_index_from_ptr(ptr, text.str, range.first);
            lr_tb_write_blank(&pos_vars, face, arena, &list, index);
        }
    }
    
    layout_item_list_finish(&list, -pos_vars.line_to_text_shift);
    
    return(list);
}

function Layout_Item_List
F4_Layout(Application_Links *app, Arena *arena, Buffer_ID buffer, Range_i64 range, Face_ID face, f32 width)
{
    return(F4_LayoutInner(app, arena, buffer, range, face, width, LayoutVirtualIndent_Off));
}

//~ NOTE(rjf): Tick

function void
F4_Tick(Application_Links *app, Frame_Info frame_info)
{
    linalloc_clear(&global_frame_arena);
    global_tooltip_count = 0;
    
    F4_TickColors(app, frame_info);
    F4_Index_Tick(app);
    F4_UpdateFlashes(app, frame_info);
    
    // NOTE(jack): Update the Pos Decay Time to reset to Center Pos None
    if (JP_CurrentSoftPos != SoftCenterPos_None)
    {
        animate_in_n_milliseconds(app, 0);
        JP_CurrentSoftPosDecayTime -= frame_info.animation_dt;
        if (JP_CurrentSoftPosDecayTime <= 0.0f) {
            JP_CurrentSoftPosDecayTime = 0.0f;
            JP_CurrentSoftPos = SoftCenterPos_None;
            // NOTE(jack): Force the tick to get recalled immediatley so that decay time is accurate
        }
        
    }
    
    View_ID view = get_active_view(app, Access_ReadVisible);
    wb_4c_tick(app, view);
    
    // NOTE(rjf): Default tick stuff from the 4th dimension:
    default_tick(app, frame_info);
}

//~ NOTE(rjf): Buffer Edit Range Hook

function BUFFER_EDIT_RANGE_SIG(F4_BufferEditRange)
{
    // buffer_id, new_range, original_size
    ProfileScope(app, "[F4] Buffer Edit Range");
    
    Range_i64 old_range = Ii64(old_cursor_range.min.pos, old_cursor_range.max.pos);
    
    buffer_shift_fade_ranges(buffer_id, old_range.max, (new_range.max - old_range.max));
    
    {
        code_index_lock();
        Code_Index_File *file = code_index_get_file(buffer_id);
        if (file != 0){
            code_index_shift(file, old_range, range_size(new_range));
        }
        code_index_unlock();
    }
    
    i64 insert_size = range_size(new_range);
    i64 text_shift = replace_range_shift(old_range, insert_size);
    
    Scratch_Block scratch(app);
    
    Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
    Async_Task *lex_task_ptr = scope_attachment(app, scope, buffer_lex_task, Async_Task);
    
    Base_Allocator *allocator = managed_scope_allocator(app, scope);
    b32 do_full_relex = false;
    
    if (async_task_is_running_or_pending(&global_async_system, *lex_task_ptr)){
        async_task_cancel(app, &global_async_system, *lex_task_ptr);
        buffer_unmark_as_modified(buffer_id);
        do_full_relex = true;
        *lex_task_ptr = 0;
    }
    
    Token_Array *ptr = scope_attachment(app, scope, attachment_tokens, Token_Array);
    if (ptr != 0 && ptr->tokens != 0){
        ProfileBlockNamed(app, "attempt resync", profile_attempt_resync);
        
        i64 token_index_first = token_relex_first(ptr, old_range.first, 1);
        i64 token_index_resync_guess =
            token_relex_resync(ptr, old_range.one_past_last, 16);
        
        if (token_index_resync_guess - token_index_first >= 4000){
            do_full_relex = true;
        }
        else{
            Token *token_first = ptr->tokens + token_index_first;
            Token *token_resync = ptr->tokens + token_index_resync_guess;
            
            Range_i64 relex_range = Ii64(token_first->pos, token_resync->pos + token_resync->size + text_shift);
            String_Const_u8 partial_text = push_buffer_range(app, scratch, buffer_id, relex_range);
            
            //~ NOTE(rjf): Lex
            F4_Language *language = F4_LanguageFromBuffer(app, buffer_id);
            // NOTE(rjf): Fall back to C++ if we don't have a proper language.
            if(language == 0)
            {
                language = F4_LanguageFromString(S8Lit("cpp"));
            }
            Token_List relex_list = F4_Language_LexFullInput_NoBreaks(app, language, scratch, partial_text);
            //~
            
            if (relex_range.one_past_last < buffer_get_size(app, buffer_id)){
                token_drop_eof(&relex_list);
            }
            
            Token_Relex relex = token_relex(relex_list, relex_range.first - text_shift, ptr->tokens, token_index_first, token_index_resync_guess);
            
            ProfileCloseNow(profile_attempt_resync);
            
            if (!relex.successful_resync){
                do_full_relex = true;
            }
            else{
                ProfileBlock(app, "apply resync");
                
                i64 token_index_resync = relex.first_resync_index;
                
                Range_i64 head = Ii64(0, token_index_first);
                Range_i64 replaced = Ii64(token_index_first, token_index_resync);
                Range_i64 tail = Ii64(token_index_resync, ptr->count);
                i64 resynced_count = (token_index_resync_guess + 1) - token_index_resync;
                i64 relexed_count = relex_list.total_count - resynced_count;
                i64 tail_shift = relexed_count - (token_index_resync - token_index_first);
                
                i64 new_tokens_count = ptr->count + tail_shift;
                Token *new_tokens = base_array(allocator, Token, new_tokens_count);
                
                Token *old_tokens = ptr->tokens;
                block_copy_array_shift(new_tokens, old_tokens, head, 0);
                token_fill_memory_from_list(new_tokens + replaced.first, &relex_list, relexed_count);
                for (i64 i = 0, index = replaced.first; i < relexed_count; i += 1, index += 1){
                    new_tokens[index].pos += relex_range.first;
                }
                for (i64 i = tail.first; i < tail.one_past_last; i += 1){
                    old_tokens[i].pos += text_shift;
                }
                block_copy_array_shift(new_tokens, ptr->tokens, tail, tail_shift);
                
                base_free(allocator, ptr->tokens);
                
                ptr->tokens = new_tokens;
                ptr->count = new_tokens_count;
                ptr->max = new_tokens_count;
                
                buffer_mark_as_modified(buffer_id);
            }
        }
    }
    
    if (do_full_relex){
        *lex_task_ptr = async_task_no_dep(&global_async_system, F4_DoFullLex_ASYNC,
                                          make_data_struct(&buffer_id));
    }
    
    // no meaning for return
    return(0);
}
