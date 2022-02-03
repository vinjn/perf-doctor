/*
 Cinder-ImGui
 This code is intended for use with Cinder
 and Omar Cornut ImGui C++ libraries.
 
 http://libcinder.org
 https://github.com/ocornut
 
 Copyright (c) 2013-2015, Simon Geilfus - All rights reserved.
 
 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:
 
 * Redistributions of source code must retain the above copyright notice, this list of conditions and
 the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
 the following disclaimer in the documentation and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#include <memory>
#include <vector>
#include <map>

#include "cinder/app/App.h"
#include "cinder/CinderAssert.h"
#include "cinder/Color.h"
#include "cinder/Noncopyable.h"
#include "cinder/Vector.h"
#include "cinder/Filesystem.h"

// forward declarations
namespace cinder {
	typedef std::shared_ptr<class DataSource> DataSourceRef;
	namespace app { typedef std::shared_ptr<class Window> WindowRef; }
	namespace gl { typedef std::shared_ptr<class Texture2d> Texture2dRef; }
}

// Custom implicit cast operators
#ifndef CINDER_IMGUI_NO_IMPLICIT_CASTS
#define IM_VEC2_CLASS_EXTRA                                             \
ImVec2(const glm::vec2& f) { x = f.x; y = f.y; }                        \
operator glm::vec2() const { return glm::vec2(x,y); }                   \
ImVec2(const glm::ivec2& f) { x = f.x; y = f.y; }                       \
operator glm::ivec2() const { return glm::ivec2(x,y); }

#define IM_VEC4_CLASS_EXTRA                                             \
ImVec4(const glm::vec4& f) { x = f.x; y = f.y; z = f.z; w = f.w; }      \
operator glm::vec4() const { return glm::vec4(x,y,z,w); }               \
ImVec4(const ci::ColorA& f) { x = f.r; y = f.g; z = f.b; w = f.a; }     \
operator ci::ColorA() const { return ci::ColorA(x,y,z,w); }             \
ImVec4(const ci::Color& f) { x = f.r; y = f.g; z = f.b; w = 1.0f; }     \
operator ci::Color() const { return ci::Color(x,y,z); }
#endif

#include "CinderImGuiExports.h"
#include "imgui.h"

#ifndef CINDER_IMGUI_NO_NAMESPACE_ALIAS
namespace ui = ImGui;
#endif

#define CINDER_IMGUI_TEXTURE_UNIT 8

//! cinder imgui namespace
namespace ImGui {
	
	struct IMGUI_API Options {
		//! defaults to using the current window, the basic ImGui font and the dark theme
		Options();
		
		//! sets the window that will be used to connect the signals and render ImGui
		Options& window( const ci::app::WindowRef &window );
		//! species whether the block should call ImGui::NewFrame and ImGui::Render automatically. Default to true.
		Options& autoRender( bool autoRender );
		
		//! sets the font to use in ImGui
		Options& font( const ci::fs::path &fontPath, float size );
		//! sets the list of available fonts to use in ImGui
		Options& fonts( const std::vector<std::pair<ci::fs::path,float>> &fontPaths, bool merge = true );
		//! sets the font to use in ImGui
		Options& fontGlyphRanges( const std::string &name, const std::vector<ImWchar> &glyphRanges );
        //! sets global font scale
        Options& fontGlobalScale( float scale );

		//! Global alpha applies to everything in ImGui
		Options& alpha( float a );
		//! Padding within a window
		Options& windowPadding( const glm::vec2 &padding );
		//! Minimum window size
		Options& windowMinSize( const glm::vec2 &minSize );
		//! Radius of window corners rounding. Set to 0.0f to have rectangular windows
		Options& windowRounding( float rounding );
		//! Alignment for title bar text
		Options& windowTitleAlign( const glm::vec2 &align );
		//! Radius of child window corners rounding. Set to 0.0f to have rectangular windows
		Options& childRounding( float rounding );
		//! Padding within a framed rectangle (used by most widgets)
		Options& framePadding( const glm::vec2 &padding );
		//! Radius of frame corners rounding. Set to 0.0f to have rectangular frame (used by most widgets).
		Options& frameRounding( float rounding );
		//! Horizontal and vertical spacing between widgets/lines
		Options& itemSpacing( const glm::vec2 &spacing );
		//! Horizontal and vertical spacing between within elements of a composed widget (e.g. a slider and its label)
		Options& itemInnerSpacing( const glm::vec2 &spacing );
		//! Expand bounding box for touch-based system where touch position is not accurate enough (unnecessary for mouse inputs). Unfortunately we don't sort widgets so priority on overlap will always be given to the first widget running. So dont grow this too much!
		Options& touchExtraPadding( const glm::vec2 &padding );
		//! Horizontal spacing when entering a tree node
		Options& indentSpacing( float spacing );
		//! Minimum horizontal spacing between two columns
		Options& columnsMinSpacing( float minSpacing );
		//! Width of the vertical scroll bar, Height of the horizontal scrollbar
		Options& scrollBarSize( float size );
		//! Radius of grab corners for scrollbar
		Options& scrollbarRounding( float rounding );
		//! Minimum width/height of a grab box for slider/scrollbar
		Options& grabMinSize( float minSize );
		//! Radius of grabs corners rounding. Set to 0.0f to have rectangular slider grabs.
		Options& grabRounding( float rounding );
		//! Alignment of button text when button is larger than text. Defaults to (0.5f,0.5f) for horizontally+vertically centered.
		Options& buttonTextAlign( const ci::vec2 &textAlign );            
		//! Window positions are clamped to be visible within the display area by at least this amount. Only covers regular windows.
		Options& displayWindowPadding( const glm::vec2 &padding );
		//! If you cannot see the edge of your screen (e.g. on a TV) increase the safe area padding. Covers popups/tooltips as well regular windows.
		Options& displaySafeAreaPadding( const glm::vec2 &padding );
		//! Enable anti-aliasing on lines/borders. Disable if you are really tight on CPU/GPU.
		Options& antiAliasedLines( bool antiAliasing );
		//! Enable anti-aliasing on filled shapes (rounded rectangles, circles, etc.)
		Options& antiAliasedFill( bool antiAliasing );
		//! Tessellation tolerance. Decrease for highly tessellated curves (higher quality, more polygons), increase to reduce quality.
		Options& curveTessellationTol( float tessTolerance );
		
		//! sets imgui ini file path
		Options& iniPath( const ci::fs::path &path );
		
		//! sets imgui original theme
		Options& defaultTheme();
		//! sets the dark theme
		Options& darkTheme();
		//! sets theme colors
		Options& color( ImGuiCol option, const ci::ColorA &color );
		
		//! returns whether the block should call ImGui::NewFrame and ImGui::Render automatically
		bool isAutoRenderEnabled() const { return mAutoRender; }
		//! returns the window that will be use to connect the signals and render ImGui
		ci::app::WindowRef getWindow() const { return mWindow; }
		//! returns the list of available fonts to use in ImGui
		const std::vector<std::pair<ci::fs::path,float>>& getFonts() const { return mFonts; }
		//! returns the glyph ranges if available for this font
		const ImWchar* getFontGlyphRanges( const std::string &name ) const;
		//! returns whether the fonts need to be merged
		bool getFontMergeMode() const { return mMergeFonts; }
		//! returns the window that will be use to connect the signals and render ImGui
		const ImGuiStyle& getStyle() const { return mStyle; }
		//! returns imgui ini file path
		const ci::fs::path& getIniPath() const { return mIniPath; }
		
	protected:
		bool							mAutoRender;
		ImGuiStyle						mStyle;
		bool							mMergeFonts;
		std::vector<std::pair<ci::fs::path,float>>	mFonts;
		std::map<std::string,std::vector<ImWchar>>	mFontsGlyphRanges;
		ci::app::WindowRef				mWindow;
		ci::fs::path					mIniPath;
	};
	
	//! initializes ImGui and the Renderer
	IMGUI_API void    initialize( const Options &options = Options() );
	//! connects window signals to imgui events
	IMGUI_API void    connectWindow( ci::app::WindowRef window );
	//! disconnects window signals from imgui
	IMGUI_API void    disconnectWindow( ci::app::WindowRef window );
	
	// Cinder Helpers
	IMGUI_API void Image( const ci::gl::Texture2dRef &texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0,1), const ImVec2& uv1 = ImVec2(1,0), const ImVec4& tint_col = ImVec4(1,1,1,1), const ImVec4& border_col = ImVec4(0,0,0,0) );
	IMGUI_API bool ImageButton( const ci::gl::Texture2dRef &texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0,1),  const ImVec2& uv1 = ImVec2(1,0), int frame_padding = -1, const ImVec4& bg_col = ImVec4(0,0,0,1), const ImVec4& tint_col = ImVec4(1,1,1,1) );
	IMGUI_API void PushFont( const std::string& name = "" );
	
	// Std Helpers
	IMGUI_API bool Combo(const char* label, int* currIndex, const std::vector<std::string>& values, ImGuiComboFlags flags = 0);
	IMGUI_API bool ListBox( const char* label, int* current_item, const std::vector<std::string>& items, int height_in_items = -1);
	IMGUI_API bool InputText( const char* label, std::string* buf, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
	IMGUI_API bool InputTextMultiline( const char* label, std::string* buf, const ImVec2& size = ImVec2(0,0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
	
	// Getters/Setters Helpers
	template<typename T>
	bool InputText( const char* label, T *object, std::string( T::*get )() const, void( T::*set )( const std::string& ), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL );
	template<typename T>
	bool Checkbox(const char* label, T *object, bool( T::*get )() const, void( T::*set )( bool ) );
	template<typename T>
	bool DragFloat(const char* label, T *object, float( T::*get )() const, void( T::*set )( float ), float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* display_format = "%.3f", float power = 1.0f);     // If v_min >= v_max we have no bound
	template<typename T>
	bool DragFloat2(const char* label, T *object, ci::vec2( T::*get )() const, void( T::*set )( const ci::vec2& ), float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* display_format = "%.3f", float power = 1.0f);
	template<typename T>
	bool DragFloat3(const char* label, T *object, ci::vec3( T::*get )() const, void( T::*set )( const ci::vec3& ), float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* display_format = "%.3f", float power = 1.0f);
	template<typename T>
	bool DragFloat4(const char* label, T *object, ci::vec4( T::*get )() const, void( T::*set )( const ci::vec4& ), float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* display_format = "%.3f", float power = 1.0f);
	template<typename T>
	bool DragInt(const char* label, T *object, int( T::*get )() const, void( T::*set )( int ), float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* display_format = "%.0f");     // If v_min >= v_max we have no bound
	template<typename T>
	bool DragInt2(const char* label, T *object, ci::ivec2( T::*get )() const, void( T::*set )( const ci::ivec2& ), float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* display_format = "%.0f" );
	template<typename T>
	bool DragInt3(const char* label, T *object, ci::ivec3( T::*get )() const, void( T::*set )( const ci::ivec3& ), float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* display_format = "%.0f" );
	template<typename T>
	bool DragInt4(const char* label, T *object, ci::ivec4( T::*get )() const, void( T::*set )( const ci::ivec4& ), float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* display_format = "%.0f" );
	template<typename T>
	bool ColorPicker3( const char* label, T *object, ci::Color( T::*get )() const, void( T::*set )( const ci::Color& ) );
	template<typename T>
	bool ColorPicker4( const char* label, T *object, ci::ColorA( T::*get )() const, void( T::*set )( const ci::ColorA& ) );

	// Scoped objects goodness (push the state when created and pop it when destroyed)

	struct IMGUI_API ScopedWindow : public ci::Noncopyable {
		ScopedWindow( const std::string &name = "Debug", ImGuiWindowFlags flags = 0 );
		~ScopedWindow();
	};
	struct IMGUI_API ScopedChild : public ci::Noncopyable {
		ScopedChild( const std::string &name, glm::vec2 size = glm::vec2(0), bool border = false, ImGuiWindowFlags extraFlags = 0 );
		~ScopedChild();
	};
	struct IMGUI_API ScopedGroup : public ci::Noncopyable {
		ScopedGroup();
		~ScopedGroup();
	};
	struct IMGUI_API ScopedFont : public ci::Noncopyable {
		ScopedFont( ImFont* font );
		ScopedFont( const std::string &name );
		~ScopedFont();
	};
	struct IMGUI_API ScopedStyleColor : public ci::Noncopyable {
		ScopedStyleColor( ImGuiCol idx, const ImVec4& col );
		~ScopedStyleColor();
	};
	struct IMGUI_API ScopedStyleVar : public ci::Noncopyable {
		ScopedStyleVar( ImGuiStyleVar idx, float val );
		ScopedStyleVar( ImGuiStyleVar idx, const ImVec2 &val );
		~ScopedStyleVar();
	};
	struct IMGUI_API ScopedItemWidth : public ci::Noncopyable {
		ScopedItemWidth( float itemWidth );
		~ScopedItemWidth();
	};
	struct IMGUI_API ScopedTextWrapPos : public ci::Noncopyable {
		ScopedTextWrapPos( float wrapPosX = 0.0f );
		~ScopedTextWrapPos();
	};
	struct IMGUI_API ScopedId : public ci::Noncopyable {
		ScopedId( const std::string &name );
		ScopedId( const void *ptrId );
		ScopedId( const int intId );
		~ScopedId();
	};
	struct IMGUI_API ScopedMainMenuBar : public ci::Noncopyable {
		ScopedMainMenuBar();
		~ScopedMainMenuBar();
	protected:
		bool mOpened;
	};
	struct IMGUI_API ScopedMenuBar : public ci::Noncopyable {
		ScopedMenuBar();
		~ScopedMenuBar();
	protected:
		bool mOpened;
	};


	// Custom widgets
	IMGUI_API bool FilePicker( const char* label, ci::fs::path* path, bool open = true, const ci::fs::path &initialPath = ci::fs::path(), std::vector<std::string> extensions = std::vector<std::string>() );
	IMGUI_API bool IconButton( const char* icon, const ImVec2& size = ImVec2(0,0), bool frame = false );
	IMGUI_API bool IconToggle( const char* iconEnabled, const char* iconDisabled, bool *enabled, const ImVec2& size = ImVec2(0,0), bool frame = false );

	// Dock From LumixEngine
	// https://github.com/nem0/LumixEngine/blob/master/external/imgui/imgui_dock.h
	// https://github.com/ocornut/imgui/issues/351
	// #define IMGUI_DOCK
#if defined( IMGUI_DOCK )
	IMGUI_API void ShutdownDock();
	IMGUI_API void RootDock(const ImVec2& pos, const ImVec2& size);
	IMGUI_API bool BeginDock(const char* label, bool* opened = nullptr, ImGuiWindowFlags extra_flags = 0);
	IMGUI_API void EndDock();
	IMGUI_API void SetDockActive();
#endif

	// Getters/Setters Helpers Implementation
	template<typename T>
	bool InputText( const char* label, T *object, std::string( T::*get )() const, void( T::*set )( const std::string& ), ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data )
	{
		std::string text = (object->*get)();
		if( InputText( label, &text, flags, callback, user_data ) ){
			(object->*set)( text );
			return true;
		}
		return false;
	}
	template<typename T>
	bool Checkbox(const char* label, T *object, bool( T::*get )() const, void( T::*set )( bool ) )
	{
		bool value = (object->*get)();
		if( Checkbox( label, &value ) ){
			(object->*set)( value );
			return true;
		}
		return false;
	}
	
	template<typename T>
	bool DragFloat(const char* label, T *object, float( T::*get )() const, void( T::*set )( float ), float v_speed, float v_min, float v_max, const char* display_format, float power )
	{
		float value = (object->*get)();
		if( DragFloat( label, &value, v_speed, v_min, v_max, display_format, power ) ){
			(object->*set)( value );
			return true;
		}
		return false;
	}
	template<typename T>
	bool DragFloat2(const char* label, T *object, ci::vec2( T::*get )() const, void( T::*set )( const ci::vec2& ), float v_speed, float v_min, float v_max, const char* display_format, float power )
	{
		ci::vec2 value = (object->*get)();
		if( DragFloat2( label, &value[0], v_speed, v_min, v_max, display_format, power ) ){
			(object->*set)( value );
			return true;
		}
		return false;
	}
	template<typename T>
	bool DragFloat3(const char* label, T *object, ci::vec3( T::*get )() const, void( T::*set )( const ci::vec3& ), float v_speed, float v_min, float v_max, const char* display_format, float power )
	{
		ci::vec3 value = (object->*get)();
		if( DragFloat3( label, &value[0], v_speed, v_min, v_max, display_format, power ) ){
			(object->*set)( value );
			return true;
		}
		return false;
	}
	template<typename T>
	bool DragFloat4(const char* label, T *object, ci::vec4( T::*get )() const, void( T::*set )( const ci::vec4& ), float v_speed, float v_min, float v_max, const char* display_format, float power )
	{
		ci::vec4 value = (object->*get)();
		if( DragFloat4( label, &value[0], v_speed, v_min, v_max, display_format, power ) ){
			(object->*set)( value );
			return true;
		}
		return false;
	}
	
	template<typename T>
	bool DragInt(const char* label, T *object, int( T::*get )() const, void( T::*set )( int ), float v_speed, int v_min, int v_max, const char* display_format )
	{
		int value = (object->*get)();
		if( DragInt( label, &value, v_speed, v_min, v_max, display_format ) ){
			(object->*set)( value );
			return true;
		}
		return false;
	}
	template<typename T>
	bool DragInt2(const char* label, T *object, ci::ivec2( T::*get )() const, void( T::*set )( const ci::ivec2& ), float v_speed, int v_min, int v_max, const char* display_format )
	{
		ci::ivec2 value = (object->*get)();
		if( DragInt2( label, &value[0], v_speed, v_min, v_max, display_format ) ){
			(object->*set)( value );
			return true;
		}
		return false;
	}
	template<typename T>
	bool DragInt3(const char* label, T *object, ci::ivec3( T::*get )() const, void( T::*set )( const ci::ivec3& ), float v_speed, int v_min, int v_max, const char* display_format )
	{
		ci::ivec3 value = (object->*get)();
		if( DragInt3( label, &value[0], v_speed, v_min, v_max, display_format ) ){
			(object->*set)( value );
			return true;
		}
		return false;
	}
	template<typename T>
	bool DragInt4(const char* label, T *object, ci::ivec4( T::*get )() const, void( T::*set )( const ci::ivec4& ), float v_speed, int v_min, int v_max, const char* display_format )
	{
		ci::ivec4 value = (object->*get)();
		if( DragInt4( label, &value[0], v_speed, v_min, v_max, display_format ) ){
			(object->*set)( value );
			return true;
		}
		return false;
	}
	template<typename T>
	bool ColorPicker3( const char* label, T *object, ci::Color( T::*get )() const, void( T::*set )( const ci::Color& ) )
	{
		ci::Color value = (object->*get)();
		if( ColorPicker3( label, &value[0] ) ){
			(object->*set)( value );
			return true;
		}
		return false;
	}
	template<typename T>
	bool ColorPicker4( const char* label, T *object, ci::ColorA( T::*get )() const, void( T::*set )( const ci::ColorA& ) )
	{
		ci::ColorA value = (object->*get)();
		if( ColorPicker4( label, &value[0] ) ){
			(object->*set)( value );
			return true;
		}
		return false;
	}
}