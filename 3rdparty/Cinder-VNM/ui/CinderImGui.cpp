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

// Linux fixes thanks to the help of @petros, see this thread:
// https://forum.libcinder.org/#Topic/23286000002634083

#include "CinderImGui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_PLACEMENT_NEW
#include "imgui_internal.h"

#include "cinder/app/App.h"
#include "cinder/gl/scoped.h"
#include "cinder/gl/draw.h"
#include "cinder/gl/Context.h"
#include "cinder/gl/Vao.h"
#include "cinder/gl/Vbo.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/Clipboard.h"
#include "cinder/CinderAssert.h"
#include "cinder/Log.h"

using namespace std;
using namespace ci;
using namespace ci::app;


namespace ImGui {

// static variables
static bool sInitialized = false;


ImGui::Options::Options()
: mWindow( ci::app::getWindow() ),
mAutoRender( true ), mMergeFonts( true )
{
}

ImGui::Options& ImGui::Options::window( const ci::app::WindowRef &window )
{
	mWindow = window;
	return *this;
}
ImGui::Options& ImGui::Options::autoRender( bool autoRender )
{
	mAutoRender = autoRender;
	return *this;
}

ImGui::Options& ImGui::Options::font( const ci::fs::path &fontPath, float size )
{
	mFonts = { { fontPath, size } };
	return *this;
}
ImGui::Options& ImGui::Options::fonts( const std::vector<std::pair<ci::fs::path,float>> &fonts, bool merge )
{
	mFonts = fonts;
	mMergeFonts = merge;
	return *this;
}
ImGui::Options& ImGui::Options::fontGlyphRanges( const std::string &name, const vector<ImWchar> &glyphRanges )
{
	mFontsGlyphRanges[name] = glyphRanges;
	return *this;
}
ImGui::Options& ImGui::Options::fontGlobalScale(float scale)
{
    ImGui::GetIO().FontGlobalScale = scale;
    return *this;
}
ImGui::Options& ImGui::Options::alpha( float a )
{
	mStyle.Alpha = a;
	return *this;
}
ImGui::Options& ImGui::Options::windowPadding( const glm::vec2 &padding )
{
	mStyle.WindowPadding = padding;
	return *this;
}
ImGui::Options& ImGui::Options::windowMinSize( const glm::vec2 &minSize )
{
	mStyle.WindowMinSize = minSize;
	return *this;
}
ImGui::Options& ImGui::Options::windowRounding( float rounding )
{
	mStyle.WindowRounding = rounding;
	return *this;
}
ImGui::Options& ImGui::Options::windowTitleAlign( const glm::vec2 &align )
{
	mStyle.WindowTitleAlign = align;
	return *this;
}
ImGui::Options& ImGui::Options::childRounding( float rounding )
{
	mStyle.ChildRounding = rounding;
	return *this;
}

ImGui::Options& ImGui::Options::framePadding( const glm::vec2 &padding )
{
	mStyle.FramePadding = padding;
	return *this;
}
ImGui::Options& ImGui::Options::frameRounding( float rounding )
{
	mStyle.FrameRounding = rounding;
	return *this;
}
ImGui::Options& ImGui::Options::itemSpacing( const glm::vec2 &spacing )
{
	mStyle.ItemSpacing = spacing;
	return *this;
}
ImGui::Options& ImGui::Options::itemInnerSpacing( const glm::vec2 &spacing )
{
	mStyle.ItemInnerSpacing = spacing;
	return *this;
}
ImGui::Options& ImGui::Options::touchExtraPadding( const glm::vec2 &padding )
{
	mStyle.TouchExtraPadding = padding;
	return *this;
}
ImGui::Options& ImGui::Options::indentSpacing( float spacing )
{
	mStyle.IndentSpacing = spacing;
	return *this;
}
ImGui::Options& ImGui::Options::columnsMinSpacing( float minSpacing )
{
	mStyle.ColumnsMinSpacing = minSpacing;
	return *this;
}
ImGui::Options& ImGui::Options::scrollBarSize( float size )
{
	mStyle.ScrollbarSize = size;
	return *this;
}
ImGui::Options& ImGui::Options::scrollbarRounding( float rounding )
{
	mStyle.ScrollbarRounding = rounding;
	return *this;
}
ImGui::Options& ImGui::Options::grabMinSize( float minSize )
{
	mStyle.GrabMinSize = minSize;
	return *this;
}
ImGui::Options& ImGui::Options::grabRounding( float rounding )
{
	mStyle.GrabRounding = rounding;
	return *this;
}
ImGui::Options& ImGui::Options::buttonTextAlign( const ci::vec2 &textAlign )
{
	mStyle.ButtonTextAlign = textAlign;
	return *this;
}
ImGui::Options& ImGui::Options::displayWindowPadding( const glm::vec2 &padding )
{
	mStyle.DisplayWindowPadding = padding;
	return *this;
}
ImGui::Options& ImGui::Options::displaySafeAreaPadding( const glm::vec2 &padding )
{
	mStyle.DisplaySafeAreaPadding = padding;
	return *this;
}
ImGui::Options& ImGui::Options::antiAliasedLines( bool antiAliasing )
{
	mStyle.AntiAliasedLines = antiAliasing;
	return *this;
}
ImGui::Options& ImGui::Options::antiAliasedFill( bool antiAliasing )
{
	mStyle.AntiAliasedFill = antiAliasing;
	return *this;
}
ImGui::Options& ImGui::Options::curveTessellationTol( float tessTolerance )
{
	mStyle.CurveTessellationTol = tessTolerance;
	return *this;
}
ImGui::Options& ImGui::Options::iniPath( const ci::fs::path &path )
{
	mIniPath = path;
	return *this;
}

const ImWchar* ImGui::Options::getFontGlyphRanges( const std::string &name ) const
{
	if( mFontsGlyphRanges.count( name ) ) {
		return &mFontsGlyphRanges.find(name)->second[0];
	}
	else return NULL;
}

ImGui::Options& ImGui::Options::defaultTheme()
{
	mStyle = ImGuiStyle();
	
	return *this;
}

ImGui::Options& ImGui::Options::darkTheme()
{
	mStyle.WindowMinSize            = ImVec2( 160, 20 );
	mStyle.FramePadding             = ImVec2( 4, 2 );
	mStyle.ItemSpacing              = ImVec2( 6, 2 );
	mStyle.ItemInnerSpacing         = ImVec2( 6, 4 );
	mStyle.Alpha			= 0.95f;
	mStyle.WindowRounding           = 4.0f;
	mStyle.FrameRounding            = 2.0f;
	mStyle.IndentSpacing            = 6.0f;
	mStyle.ItemInnerSpacing		= ImVec2( 2, 4 );
	mStyle.ColumnsMinSpacing        = 50.0f;
	mStyle.GrabMinSize		= 14.0f;
	mStyle.GrabRounding		= 16.0f;
	mStyle.ScrollbarSize		= 12.0f;
	mStyle.ScrollbarRounding	= 16.0f;
	
	ImGuiStyle& style = mStyle;
	style.Colors[ImGuiCol_Text]                  = ImVec4(0.86f, 0.93f, 0.89f, 0.78f);
	style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.86f, 0.93f, 0.89f, 0.28f);
	style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
	style.Colors[ImGuiCol_Border]                = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);
	style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
	style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
	style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.20f, 0.22f, 0.27f, 0.75f);
	style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.20f, 0.22f, 0.27f, 0.47f);
	style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.09f, 0.15f, 0.16f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
	style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
	style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_Button]                = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
	style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
	style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_Header]                = ImVec4(0.92f, 0.18f, 0.29f, 0.76f);
	style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
	style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_Separator]                = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
	style.Colors[ImGuiCol_SeparatorHovered]         = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
	style.Colors[ImGuiCol_SeparatorActive]          = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
	style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
	style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
	style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
	style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.92f, 0.18f, 0.29f, 0.43f);
	style.Colors[ImGuiCol_PopupBg]				 = ImVec4(0.20f, 0.22f, 0.27f, 0.9f);
	style.Colors[ImGuiCol_ModalWindowDimBg]  = ImVec4(0.20f, 0.22f, 0.27f, 0.73f);
	
	return *this;
}


ImGui::Options& ImGui::Options::color( ImGuiCol option, const ci::ColorA &color )
{
	mStyle.Colors[ option ] = color;
	return *this;
}

//! cinder renderer
class Renderer {
public:
	Renderer();
	
	//! renders imgui drawlist
	void render( ImDrawData* draw_data );
	//! sets the font
	ImFont* addFont( const ci::fs::path &font, float size, const ImWchar* glyph_ranges = NULL, bool merge = true );
	
	//! initializes and returns the font texture
	ci::gl::Texture2dRef getFontTextureRef();
	//! initializes and returns the vao
	ci::gl::VaoRef getVao();
	//! initializes and returns the vbo
	ci::gl::VboRef getVbo();
	//! initializes and returns the shader
	ci::gl::GlslProgRef getGlslProg();
	
	//! initializes the font texture
	void initFontTexture();
	//! initializes the vbo mesh
	void initBuffers( size_t size = 1000 );
	//! initializes the shader
	void initGlslProg();
	
	ImFont* getFont( const std::string &name );
	void clearFonts();
	
protected:
	ci::gl::Texture2dRef	mFontTexture;
	ci::gl::VaoRef		mVao;
	ci::gl::VboRef		mVbo;
	ci::gl::VboRef		mIbo;
	ci::gl::GlslProgRef		mShader;
	
	map<string,ImFont*>		mFonts;
	vector<vector<ImWchar>>	mFontsGlyphRanges;
};



Renderer::Renderer()
{
	initGlslProg();
	initBuffers();
}

#define USE_MAP_BUFFER

//! renders imgui drawlist
void Renderer::render( ImDrawData* draw_data )
{
	const float width	= ImGui::GetIO().DisplaySize.x;
	const float height	= ImGui::GetIO().DisplaySize.y;
	const auto &vao		= getVao();
	const auto &vbo		= getVbo();
	const auto &shader	= getGlslProg();
	auto ctx			= gl::context();
	
	const mat4 mat =
	{
		{ 2.0f/width,	0.0f,		0.0f,	0.0f },
		{ 0.0f,		2.0f/-height,	0.0f,	0.0f },
		{ 0.0f,		0.0f,			-1.0f,	0.0f },
		{ -1.0f,	1.0f,			0.0f,	1.0f },
	};
	
	gl::ScopedVao scopedVao( vao.get() );
	gl::ScopedBuffer scopedVbo( GL_ARRAY_BUFFER, vbo->getId() );
	gl::ScopedBuffer scopedIbo( GL_ELEMENT_ARRAY_BUFFER, mIbo->getId() );
	gl::ScopedGlslProg scopedShader( shader );
	gl::ScopedDepth scopedDepth( false );
	gl::ScopedBlendAlpha scopedBlend;
	gl::ScopedFaceCulling scopedFaceCulling( false );
	shader->uniform( "uModelViewProjection", mat );

	GLuint currentTextureId = 0;
	ctx->pushTextureBinding( GL_TEXTURE_2D, CINDER_IMGUI_TEXTURE_UNIT );
	ctx->pushBoolState( GL_SCISSOR_TEST, GL_TRUE );
	ctx->pushScissor();
	for (int n = 0; n < draw_data->CmdListsCount; n++) {
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawIdx* idx_buffer_offset = 0;
	
#if defined( USE_MAP_BUFFER ) 
		// grow vertex buffer if needed
		int needed_vtx_size = cmd_list->VtxBuffer.size() * sizeof(ImDrawVert);
		if ( vbo->getSize() < needed_vtx_size) {
			GLsizeiptr size = needed_vtx_size + 2000 * sizeof(ImDrawVert);
#ifndef CINDER_LINUX_EGL_RPI2
			mVbo->bufferData( size, nullptr, GL_STREAM_DRAW );
#else
			mVbo->bufferData( size, nullptr, GL_DYNAMIC_DRAW );
#endif
		}

		// update vertex data
		{
			ImDrawVert *vtx_data = static_cast<ImDrawVert*>( vbo->mapReplace() );
			if (!vtx_data)
				continue;
			memcpy( vtx_data, &cmd_list->VtxBuffer[0], cmd_list->VtxBuffer.size() * sizeof(ImDrawVert) );
			vbo->unmap();
		}
		
		// grow index buffer if needed
		int needed_idx_size = cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
		if( mIbo->getSize() < needed_idx_size ) {
			GLsizeiptr size = needed_idx_size + 2000 * sizeof(ImDrawIdx);
#if ! defined( CINDER_LINUX_EGL_RPI2 )
			mIbo->bufferData( size, nullptr, GL_STREAM_DRAW );
#else
			mIbo->bufferData( size, nullptr, GL_DYNAMIC_DRAW );
#endif
		}
		
		// update index data
		{
			ImDrawIdx *idx_data = static_cast<ImDrawIdx*>( mIbo->mapReplace() );
			if( ! idx_data )
				continue;
			memcpy( idx_data, &cmd_list->IdxBuffer[0], cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx) );
			mIbo->unmap();
		}
#else
#if ! defined( CINDER_LINUX_EGL_RPI2 )
		mVbo->bufferData( (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid*)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW );
		mIbo->bufferData( (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid*)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW );
#else
		mVbo->bufferData( (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid*)cmd_list->VtxBuffer.Data, GL_DYNAMIC_DRAW );
		mIbo->bufferData( (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid*)cmd_list->IdxBuffer.Data, GL_DYNAMIC_DRAW );
#endif
#endif
		
		// issue draw commands
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback) {
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else {
				bool pushTexture = currentTextureId != (GLuint)(intptr_t) pcmd->TextureId;
				if( pushTexture ) {
					currentTextureId = (GLuint)(intptr_t) pcmd->TextureId;
					ctx->bindTexture( GL_TEXTURE_2D, currentTextureId, CINDER_IMGUI_TEXTURE_UNIT );
				}
				ctx->setScissor( { ivec2( (int)pcmd->ClipRect.x, (int)(height - pcmd->ClipRect.w) ), ivec2( (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y) ) } );
				gl::drawElements( GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset );
			}
			idx_buffer_offset += pcmd->ElemCount;
		}
	}

	ctx->popScissor();
	ctx->popBoolState( GL_SCISSOR_TEST );
	ctx->popTextureBinding( GL_TEXTURE_2D, CINDER_IMGUI_TEXTURE_UNIT );
}

//! initializes and returns the font texture
gl::TextureRef Renderer::getFontTextureRef()
{
	if( !mFontTexture ){
		initFontTexture();
	}
	return mFontTexture;
}
//! initializes and returns the vbo mesh
gl::VaoRef Renderer::getVao()
{
	if( !mVao ){
		initBuffers();
	}
	return mVao;
}

//! initializes and returns the vbo
gl::VboRef Renderer::getVbo()
{
	if( !mVbo ){
		initBuffers();
	}
	return mVbo;
}

//! initializes the vbo mesh
void Renderer::initBuffers( size_t size )
{
	#if ! defined( CINDER_LINUX_EGL_RPI2 )
	mVbo    = gl::Vbo::create( GL_ARRAY_BUFFER, size, nullptr, GL_STREAM_DRAW );
	mIbo    = gl::Vbo::create( GL_ELEMENT_ARRAY_BUFFER, 10, nullptr, GL_STREAM_DRAW );
	#else
	mVbo    = gl::Vbo::create( GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW );
	mIbo    = gl::Vbo::create( GL_ELEMENT_ARRAY_BUFFER, 10, nullptr, GL_DYNAMIC_DRAW );
	#endif
	mVao    = gl::Vao::create();
	
	gl::ScopedVao mVaoScope( mVao );
	gl::ScopedBuffer mVboScope( mVbo );
	
	gl::enableVertexAttribArray( 0 );
	gl::enableVertexAttribArray( 1 );
	gl::enableVertexAttribArray( 2 );
	
	gl::vertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (const GLvoid*)offsetof(ImDrawVert, pos) );
	gl::vertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (const GLvoid*)offsetof(ImDrawVert, uv) );
	gl::vertexAttribPointer( 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (const GLvoid*)offsetof(ImDrawVert, col) );
	
}
//! initalizes the font texture
void Renderer::initFontTexture()
{
	unsigned char* pixels;
	int width, height;
	ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	mFontTexture = gl::Texture::create( pixels, GL_RGBA, width, height, gl::Texture::Format().magFilter(GL_LINEAR).minFilter(GL_LINEAR) );
	ImGui::GetIO().Fonts->ClearTexData();
	ImGui::GetIO().Fonts->TexID = (void *)(intptr_t) mFontTexture->getId();
}

/*ImFont* addFont( ci::DataSourceRef font, float size, int fontId )
 {
 ImFont* newFont = getRenderer()->addFont( font, size, fontId );
 return newFont;
 }
 */

ImFont* Renderer::addFont( const ci::fs::path &font, float size, const ImWchar* glyph_ranges, bool merge )
{
	ImFontAtlas* fontAtlas  = ImGui::GetIO().Fonts;
	
	auto fontSource = loadFile( font );
	Font ciFont( fontSource, size );

	ImWchar* glyphRanges = NULL;
	// if we have glyph ranges copy them
	if( glyph_ranges && glyph_ranges[0] && glyph_ranges[1] ){
		mFontsGlyphRanges.push_back( vector<ImWchar>() );
		auto &ranges = mFontsGlyphRanges.back();
		for( const ImWchar* range = glyph_ranges; range[0] && range[1]; range += 2 ) {
			ranges.push_back( range[0] );
			ranges.push_back( range[1] );
		}
		ranges.push_back( 0 );
		glyphRanges = ranges.data();
	}
	// otherwise get them from the font
	else if( ciFont.getNumGlyphs() ) {
		int numGlyphs = ciFont.getNumGlyphs();
		// copy glyphs so we can sort them
		vector<ImWchar> glyphs;
		for( size_t i = 0; i < numGlyphs; ++i ) { 
			glyphs.push_back( ciFont.getGlyphIndex( i ) );
		}
		sort( glyphs.begin(), glyphs.end() );
	
		// find glyph ranges
		mFontsGlyphRanges.push_back( vector<ImWchar>() );
		auto &ranges = mFontsGlyphRanges.back();
		Font::Glyph start = glyphs[0] == 0 ? '0' : glyphs[0];
		for( size_t i = 1; i < numGlyphs; ++i ) {
			if( glyphs[i] != glyphs[i-1] + 1 ) {
				ranges.push_back( start );
				ranges.push_back( glyphs[i-1] );
				start = glyphs[i];
			}
		}
		if( ranges.empty() || ranges.back() != glyphs[numGlyphs-1] ) {
			ranges.push_back( start );
			ranges.push_back( glyphs[numGlyphs-1] );
		}
		ranges.push_back( '\0' );
		glyphRanges = ranges.data();
	}
	
	BufferRef buffer	= fontSource->getBuffer();
	void* bufferCopy	= (void*) malloc( buffer->getSize() );
	memcpy( bufferCopy, buffer->getData(), buffer->getSize() );
	
	ImFontConfig config;
	if( merge && ! mFonts.empty() ) {
		config.MergeMode = true;
		config.PixelSnapH = true;
		//config.OversampleV = 4;
	}
	/*else if( merge ) {
		fontAtlas->AddFontDefault();
		config.MergeMode = true;
		config.PixelSnapH = true;
	}*/

	ImFont* newFont = fontAtlas->AddFontFromMemoryTTF( bufferCopy, buffer->getSize(), size, &config, glyphRanges );
	//ImFont* newFont = fontAtlas->AddFontFromFileTTF( font.string().c_str(), size, &config, glyphRanges );
	
	mFonts.insert( make_pair( font.stem().string(), newFont ) );
	return newFont;
}
void Renderer::clearFonts()
{
	mFonts.clear();
}

//! initalizes and returns the shader
gl::GlslProgRef Renderer::getGlslProg()
{
	if( !mShader ){
		initGlslProg();
	}
	return mShader;
}

//! initalizes the shader
void Renderer::initGlslProg()
{
	try {
		mShader = gl::GlslProg::create( gl::GlslProg::Format()
					       .vertex(
					#if defined(CINDER_GL_ES_2)
						       R"(
						       precision highp float;
						       uniform mat4 uModelViewProjection;
						       
						       attribute vec2      iPosition;
						       attribute vec2      iUv;
						       attribute vec4      iColor;
						       
						       varying vec2     vUv;
						       varying vec4     vColor;
						       
						       void main() {
							       vColor       = iColor;
							       vUv          = iUv;
							       gl_Position  = uModelViewProjection * vec4( iPosition, 0.0, 1.0 );
						       } )"
					#elif defined(CINDER_GL_ES_3)
					       R"(
						#version 300 es
					       precision highp float;
					       uniform mat4 uModelViewProjection;
					       
					       in vec2      iPosition;
					       in vec2      iUv;
					       in vec4      iColor;
					       
					       out vec2     vUv;
					       out vec4     vColor;
					       
					       void main() {
						       vColor       = iColor;
						       vUv          = iUv;
						       gl_Position  = uModelViewProjection * vec4( iPosition, 0.0, 1.0 );
					       } )"
					#else
						R"(
						#version 150
						uniform mat4 uModelViewProjection;
						in vec2      iPosition;
						in vec2      iUv;
						in vec4      iColor;
						out vec2     vUv;
						out vec4     vColor;
						void main() {
							vColor       = iColor;
							vUv          = iUv;
							gl_Position  = uModelViewProjection * vec4( iPosition, 0.0, 1.0 );
						} )"
						
					#endif
		)
		.fragment(
	#if defined(CINDER_GL_ES_2)
			  R"(
			  precision highp float;
			  
			  varying highp vec2	vUv;
			  varying highp vec4	vColor;
			  uniform sampler2D	uTex;
			  
			  void main() {
				  vec4 color = texture2D( uTex, vUv ) * vColor;
				  gl_FragColor = color;
			  }  )"
	#elif defined(CINDER_GL_ES_3)
		R"(
		#version 300 es
		precision highp float;
		
		in highp vec2		vUv;
		in highp vec4		vColor;
		out highp vec4		oColor;
		uniform sampler2D	uTex;
		
		void main() {
			vec4 color = texture( uTex, vUv ) * vColor;
			oColor = color;
		}  )"
	#else
		R"(
		#version 150
		
		in vec2			vUv;
		in vec4			vColor;
		out vec4		oColor;
		uniform sampler2D	uTex;
		
		void main() {
			vec4 color = texture( uTex, vUv ) * vColor;
			oColor = color;
		}  )"
		
	#endif
		)
		.attribLocation( "iPosition", 0 )
		.attribLocation( "iUv", 1 )
		.attribLocation( "iColor", 2 )
		);

		mShader->uniform( "uTex", CINDER_IMGUI_TEXTURE_UNIT );

	}
	catch( gl::GlslProgCompileExc exc ){
		CI_LOG_E( "Problem Compiling ImGui::Renderer shader " << exc.what() );
	}
}
ImFont* Renderer::getFont( const std::string &name )
{
	if( !mFonts.count( name ) ){
		return nullptr;
	}
	else {
		return mFonts[name];
	}
}


typedef std::shared_ptr<Renderer> RendererRef;
RendererRef getRenderer()
{
	static RendererRef renderer = RendererRef( new Renderer() );
	return renderer;
}


// Cinder Helpers
void Image( const ci::gl::Texture2dRef &texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col )
{
	Image( (void*)(intptr_t) texture->getId(), size, uv0, uv1, tint_col, border_col );
}
bool ImageButton( const ci::gl::Texture2dRef &texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col)
{
	return ImageButton( (void*)(intptr_t) texture->getId(), size, uv0, uv1, frame_padding, bg_col, tint_col );
}

void PushFont( const std::string& name )
{
	auto renderer   = getRenderer();
	ImFont* font    = renderer->getFont( name );
	CI_ASSERT( font != nullptr );
	PushFont( font );
}

bool Combo(const char* label, int* currIndex, const std::vector<std::string>& values, ImGuiComboFlags flags)
{
	if (values.empty()) return false;

	bool changed = false;
	int itemsCount = (int)values.size();
	const char* previewItem = NULL;
	if (*currIndex >= 0 && *currIndex < itemsCount) {
		previewItem = values.at(*currIndex).c_str();
	}

	if (ImGui::BeginCombo(label, previewItem, flags)) {
		for (int i = 0; i < itemsCount; ++i) {
			ImGui::PushID((void*)(intptr_t)i);
			bool selected = (*currIndex == i);
			if (ImGui::Selectable(values.at(i).c_str(), selected)) {
				*currIndex = i;
				changed = true;
			}
			if (selected) ImGui::SetItemDefaultFocus();
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
	return changed;
}

// Std Helpers
bool ListBox( const char* label, int* current_item, const std::vector<std::string>& items, int height_in_items )
{
	// copy names to a vector
	vector<const char*> names;
	for( auto item : items ) {
		char *cname = new char[ item.size() + 1 ];
		std::strcpy( cname, item.c_str() );
		names.push_back( cname );
	}
	
	bool result = ListBox( label, current_item, names.data(), names.size(), height_in_items );
	// cleanup
	for( auto &name : names ) delete [] name;
	return result;
}
bool InputText( const char* label, std::string* buf, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data )
{
	// conversion
	char *buffer = new char[buf->size()+128];
	std::strcpy( buffer, buf->c_str() );
	bool result = InputText( label, buffer, buf->size()+128, flags, callback, user_data );
	if( result ){
		*buf = string( buffer );
	}
	// cleanup
	delete [] buffer;
	return result;
}
bool InputTextMultiline( const char* label, std::string* buf, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data )
{
	// conversion
	constexpr size_t extraSpace = 16384;
	char *buffer = new char[buf->size()+extraSpace];
	std::strcpy( buffer, buf->c_str() );
	bool result = InputTextMultiline( label, buffer, buf->size()+extraSpace, size, flags, callback, user_data );
	if( result ){
		*buf = string( buffer );
	}
	// cleanup
	delete [] buffer;
	return result;
}

namespace {
	
	//! sets the right mouseDown IO values in imgui
	void mouseDown( ci::app::MouseEvent& event )
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MousePos = toPixels( event.getPos() );
		if( event.isLeftDown() ){
			io.MouseDown[0] = true;
			io.MouseDown[1] = false;
		}
		else if( event.isRightDown() ){
			io.MouseDown[0] = false;
			io.MouseDown[1] = true;
		}
		
		event.setHandled( io.WantCaptureMouse );
	}
	//! sets the right mouseMove IO values in imgui
	void mouseMove( ci::app::MouseEvent& event )
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MousePos = toPixels( event.getPos() );
		
		event.setHandled( io.WantCaptureMouse );
	}
	//! sets the right mouseDrag IO values in imgui
	void mouseDrag( ci::app::MouseEvent& event )
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MousePos = toPixels( event.getPos() );
		
		event.setHandled( io.WantCaptureMouse );
	}
	//! sets the right mouseDrag IO values in imgui
	void mouseUp( ci::app::MouseEvent& event )
	{
		ImGuiIO& io     = ImGui::GetIO();
		io.MouseDown[0] = false;
		io.MouseDown[1] = false;
		
		event.setHandled( io.WantCaptureMouse );
	}
	//! sets the right mouseWheel IO values in imgui
	void mouseWheel( ci::app::MouseEvent& event )
	{
		ImGuiIO& io     = ImGui::GetIO();
		io.MouseWheel   += event.getWheelIncrement();
		
		event.setHandled( io.WantCaptureMouse );
	}
	
	
	vector<int> sAccelKeys;
	
	//! sets the right keyDown IO values in imgui
	void keyDown( ci::app::KeyEvent& event )
	{
		ImGuiIO& io = ImGui::GetIO();

		#if defined CINDER_LINUX
			auto character = event.getChar();
		#else
			uint32_t character = event.getCharUtf32();
		#endif

		io.KeysDown[event.getCode()] = true;
		
		if ( !event.isAccelDown() && character > 0 && character <= 255 ) {
			io.AddInputCharacter( ( char ) character );
		}
		else if( event.getCode() != KeyEvent::KEY_LMETA
			&& event.getCode() != KeyEvent::KEY_RMETA
			&& event.isAccelDown()
			&& find( sAccelKeys.begin(), sAccelKeys.end(), event.getCode() ) == sAccelKeys.end() ){
			sAccelKeys.push_back( event.getCode() );
		}
		
		io.KeyCtrl = io.KeysDown[KeyEvent::KEY_LCTRL] || io.KeysDown[KeyEvent::KEY_RCTRL];
		io.KeyShift = io.KeysDown[KeyEvent::KEY_LSHIFT] || io.KeysDown[KeyEvent::KEY_RSHIFT];
		io.KeyAlt = io.KeysDown[KeyEvent::KEY_LALT] || io.KeysDown[KeyEvent::KEY_RALT];
		io.KeySuper = io.KeysDown[KeyEvent::KEY_LMETA] || io.KeysDown[KeyEvent::KEY_RMETA] || io.KeysDown[KeyEvent::KEY_LSUPER] || io.KeysDown[KeyEvent::KEY_RSUPER];
		
		event.setHandled( io.WantCaptureKeyboard );
	}
	//! sets the right keyUp IO values in imgui
	void keyUp( ci::app::KeyEvent& event )
	{
		ImGuiIO& io = ImGui::GetIO();
		
		io.KeysDown[event.getCode()] = false;
		
		for( auto key : sAccelKeys ){
			io.KeysDown[key] = false;
		}
		sAccelKeys.clear();
		
		io.KeyCtrl = io.KeysDown[KeyEvent::KEY_LCTRL] || io.KeysDown[KeyEvent::KEY_RCTRL];
		io.KeyShift = io.KeysDown[KeyEvent::KEY_LSHIFT] || io.KeysDown[KeyEvent::KEY_RSHIFT];
		io.KeyAlt = io.KeysDown[KeyEvent::KEY_LALT] || io.KeysDown[KeyEvent::KEY_RALT];
		io.KeySuper = io.KeysDown[KeyEvent::KEY_LMETA] || io.KeysDown[KeyEvent::KEY_RMETA] || io.KeysDown[KeyEvent::KEY_LSUPER] || io.KeysDown[KeyEvent::KEY_RSUPER];
		
		event.setHandled( io.WantCaptureKeyboard );
	}
	
	static bool sNewFrame = false;
	void newFrameGuard();
	void render()
	{
		static auto timer = ci::Timer(true);
		ImGuiIO& io = ImGui::GetIO();
		io.DeltaTime = timer.getSeconds();

		if( io.DeltaTime < 0.0f ) {
			CI_LOG_W("WARNING: overriding imgui deltatime because it is " << io.DeltaTime);
			io.DeltaTime = 1.0f/60.0f;
		}

		timer.start();
		ImGui::Render();
		auto renderer = getRenderer();
		renderer->render( ImGui::GetDrawData() );
		sNewFrame = false;
		App::get()->dispatchAsync( []() {
			newFrameGuard();
		} );
	}
	
	void newFrameGuard()
	{
		if( ! sNewFrame ){
			ImGui::NewFrame();
			sNewFrame = true;
		}
	}

	void resize()
	{
		ImGuiIO& io	= ImGui::GetIO();
		io.DisplaySize	= toPixels( getWindowSize() );
		newFrameGuard();
	}
	
	void resetKeys()
	{
		for( int i = 0; i < IM_ARRAYSIZE( ImGui::GetIO().KeysDown ); i++ ) {
			ImGui::GetIO().KeysDown[i] = false;
		}
		for( int i = 0; i < IM_ARRAYSIZE( ImGui::GetIO().KeysDown); i++ ) {
			ImGui::GetKeyData(i)->DownDuration = 0.0f;
		}
		for( int i = 0; i < IM_ARRAYSIZE( ImGui::GetIO().KeysDown); i++ ) {
			ImGui::GetKeyData(i)->DownDurationPrev = 0.0f;
		}
		ImGui::GetIO().KeyCtrl	= false;
		ImGui::GetIO().KeyShift = false;
		ImGui::GetIO().KeyAlt	= false;
	}
	
} // Anonymous namespace

// wrong... and would not work in a multi-windows scenario
static signals::ConnectionList sWindowConnections;

// also wrong... but fixes a crash on cleanup
static signals::ConnectionList sAppConnections;

void initialize( const Options &options )
{
	// create one context for now. will update with multiple context / shared fontatlas soon!
	ImGuiContext* context = ImGui::CreateContext();

	// get the window and switch to its context before initializing the renderer
	auto window					= options.getWindow();
	auto currentContext			= gl::context();
	window->getRenderer()->makeCurrentContext();
	auto renderer				= getRenderer();
	
	// set style
	const ImGuiStyle& style				= options.getStyle();
	ImGuiStyle& imGuiStyle				= ImGui::GetStyle();
	imGuiStyle.Alpha					= style.Alpha;
	imGuiStyle.WindowPadding			= style.WindowPadding;
	imGuiStyle.WindowMinSize			= style.WindowMinSize;
	imGuiStyle.WindowRounding			= style.WindowRounding;
	imGuiStyle.WindowTitleAlign			= style.WindowTitleAlign;
	imGuiStyle.ChildRounding			= style.ChildRounding;
	imGuiStyle.FramePadding				= style.FramePadding;
	imGuiStyle.FrameRounding			= style.FrameRounding;
	imGuiStyle.ItemSpacing				= style.ItemSpacing;
	imGuiStyle.ItemInnerSpacing			= style.ItemInnerSpacing;
	imGuiStyle.TouchExtraPadding		= style.TouchExtraPadding;
	imGuiStyle.IndentSpacing			= style.IndentSpacing;
	imGuiStyle.ColumnsMinSpacing		= style.ColumnsMinSpacing;
	imGuiStyle.ScrollbarSize			= style.ScrollbarSize;
	imGuiStyle.ScrollbarRounding		= style.ScrollbarRounding;
	imGuiStyle.GrabMinSize				= style.GrabMinSize;
	imGuiStyle.GrabRounding				= style.GrabRounding;
	imGuiStyle.ButtonTextAlign			= style.ButtonTextAlign;
	imGuiStyle.DisplayWindowPadding		= style.DisplayWindowPadding;
	imGuiStyle.DisplaySafeAreaPadding	= style.DisplaySafeAreaPadding;
	imGuiStyle.AntiAliasedLines			= style.AntiAliasedLines;
	imGuiStyle.AntiAliasedFill			= style.AntiAliasedFill;
	
	// set colors
	for( int i = 0; i < ImGuiCol_COUNT; i++ )
		imGuiStyle.Colors[i] = style.Colors[i];
	
	// set io and keymap
	ImGuiIO& io                         = ImGui::GetIO();
	io.DisplaySize                      = ImVec2( window->getSize().x, window->getSize().y );
	io.DeltaTime                        = 1.0f / 60.0f;
	io.KeyMap[ImGuiKey_Tab]             = KeyEvent::KEY_TAB;
	io.KeyMap[ImGuiKey_LeftArrow]       = KeyEvent::KEY_LEFT;
	io.KeyMap[ImGuiKey_RightArrow]      = KeyEvent::KEY_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow]         = KeyEvent::KEY_UP;
	io.KeyMap[ImGuiKey_DownArrow]       = KeyEvent::KEY_DOWN;
	io.KeyMap[ImGuiKey_Home]            = KeyEvent::KEY_HOME;
	io.KeyMap[ImGuiKey_End]             = KeyEvent::KEY_END;
	io.KeyMap[ImGuiKey_Delete]          = KeyEvent::KEY_DELETE;
	io.KeyMap[ImGuiKey_Backspace]       = KeyEvent::KEY_BACKSPACE;
	io.KeyMap[ImGuiKey_Enter]           = KeyEvent::KEY_RETURN;
	io.KeyMap[ImGuiKey_Escape]          = KeyEvent::KEY_ESCAPE;
	io.KeyMap[ImGuiKey_A]               = KeyEvent::KEY_a;
	io.KeyMap[ImGuiKey_C]               = KeyEvent::KEY_c;
	io.KeyMap[ImGuiKey_V]               = KeyEvent::KEY_v;
	io.KeyMap[ImGuiKey_X]               = KeyEvent::KEY_x;
	io.KeyMap[ImGuiKey_Y]               = KeyEvent::KEY_y;
	io.KeyMap[ImGuiKey_Z]               = KeyEvent::KEY_z;
	io.KeyMap[ImGuiKey_Insert]			= KeyEvent::KEY_INSERT;
	io.KeyMap[ImGuiKey_Space]			= KeyEvent::KEY_SPACE;
	
	// setup config file path
	static string path = ( getAssetPath( "" ) / "imgui.ini" ).string();
	if( ! options.getIniPath().empty() )
		path = options.getIniPath().string().c_str();
	io.IniFilename = path.c_str();
	
	// setup fonts
	ImFontAtlas* fontAtlas  = ImGui::GetIO().Fonts;
	fontAtlas->Clear();
	renderer->clearFonts();
	for( auto font : options.getFonts() ){
		string name = font.first.stem().string();
		renderer->addFont( font.first, font.second, options.getFontGlyphRanges( name ), options.getFontMergeMode() );
	}
	renderer->initFontTexture();
	
#ifndef CINDER_LINUX
	// clipboard callbacks
	io.SetClipboardTextFn = []( void* user_data, const char* text ) {
		// clipboard text is already zero-terminated
		Clipboard::setString( text );
	};
	io.GetClipboardTextFn = []( void* user_data ) {
		string str = Clipboard::getString();
		static vector<char> strCopy;
		strCopy = vector<char>(str.begin(), str.end());
		strCopy.push_back('\0');
		return (const char *) &strCopy[0];
	};
#endif
	
	// connect window's signals
	disconnectWindow( window );
	connectWindow( window );
	
	if( options.isAutoRenderEnabled() && window ) {
		sWindowConnections += ( window->getSignalDraw().connect( newFrameGuard ) );
		sWindowConnections += ( window->getSignalPostDraw().connect( render ) );
	}
	
	// connect app's signals
    sAppConnections += app::App::get()->getSignalDidBecomeActive().connect( resetKeys );
    sAppConnections += app::App::get()->getSignalWillResignActive().connect( resetKeys );
    sAppConnections += app::App::get()->getSignalCleanup().connect( [context](){
        sAppConnections.clear();

		ImGui::DestroyContext( context );
#if defined( IMGUI_DOCK )
		ShutdownDock();
#endif
	} );

	sInitialized = true;
	
	// switch back to the original gl context
	currentContext->makeCurrent();
}


void connectWindow( ci::app::WindowRef window )
{
	sWindowConnections += window->getSignalMouseDown().connect( mouseDown );
	sWindowConnections += window->getSignalMouseUp().connect( mouseUp );
	sWindowConnections += window->getSignalMouseDrag().connect( mouseDrag );
	sWindowConnections += window->getSignalMouseMove().connect( mouseMove );
	sWindowConnections += window->getSignalMouseWheel().connect( mouseWheel );
	sWindowConnections += window->getSignalKeyDown().connect( keyDown );
	sWindowConnections += window->getSignalKeyUp().connect( keyUp );
	sWindowConnections += window->getSignalResize().connect( resize );
}
void disconnectWindow( ci::app::WindowRef window )
{
	sWindowConnections.clear();
}

ScopedWindow::ScopedWindow( const std::string &name, ImGuiWindowFlags flags)
{
	ImGui::Begin( name.c_str(), nullptr, flags );
}
ScopedWindow::~ScopedWindow()
{
	ImGui::End();
}
ScopedChild::ScopedChild( const std::string &name, glm::vec2 size, bool border, ImGuiWindowFlags extraFlags )
{
	ImGui::BeginChild( name.c_str(), size, border, extraFlags );
}
ScopedChild::~ScopedChild()
{
	ImGui::EndChild();
}
ScopedGroup::ScopedGroup()
{
	ImGui::BeginGroup();
}
ScopedGroup::~ScopedGroup()
{
	ImGui::EndGroup();
}
ScopedFont::ScopedFont( ImFont* font )
{
	ImGui::PushFont( font );
}
ScopedFont::ScopedFont( const std::string &name )
{
	ImGui::PushFont( name );
}
ScopedFont::~ScopedFont()
{
	ImGui::PopFont();
}
ScopedStyleColor::ScopedStyleColor( ImGuiCol idx, const ImVec4& col )
{
	ImGui::PushStyleColor( idx, col );
}
ScopedStyleColor::~ScopedStyleColor()
{
	ImGui::PopStyleColor();
}
ScopedStyleVar::ScopedStyleVar( ImGuiStyleVar idx, float val )
{
	ImGui::PushStyleVar( idx, val );
}
ScopedStyleVar::ScopedStyleVar( ImGuiStyleVar idx, const ImVec2 &val )
{
	ImGui::PushStyleVar( idx, val );
}
ScopedStyleVar::~ScopedStyleVar()
{
	ImGui::PopStyleVar();
}

ScopedItemWidth::ScopedItemWidth( float itemWidth )
{
	ImGui::PushItemWidth( itemWidth );
}
ScopedItemWidth::~ScopedItemWidth()
{
	ImGui::PopItemWidth();
}

ScopedTextWrapPos::ScopedTextWrapPos( float wrapPosX )
{
	ImGui::PushTextWrapPos( wrapPosX );
}
ScopedTextWrapPos::~ScopedTextWrapPos()
{
	ImGui::PopTextWrapPos();
}

ScopedId::ScopedId( const std::string &name )
{
	ImGui::PushID( name.c_str() );
}
ScopedId::ScopedId( const void *ptrId )
{
	ImGui::PushID( ptrId );
}
ScopedId::ScopedId( const int intId )
{
	ImGui::PushID( intId );
}
ScopedId::~ScopedId()
{
	ImGui::PopID();
}

ScopedMainMenuBar::ScopedMainMenuBar()
{
	mOpened = ImGui::BeginMainMenuBar();
}
ScopedMainMenuBar::~ScopedMainMenuBar()
{
	if( mOpened ) ImGui::EndMainMenuBar();
}

ScopedMenuBar::ScopedMenuBar()
{
	mOpened = ImGui::BeginMenuBar();
}
ScopedMenuBar::~ScopedMenuBar()
{
	if( mOpened ) ImGui::EndMenuBar();
}
//
//bool FilePicker( const char* label, fs::path* path, bool open, const fs::path &initialPath, std::vector<std::string> extensions )
//{
//
//    ImGuiWindow* window = GetCurrentWindow();
//    if (window->SkipItems)
//        return false;
//
//    ImGuiContext& g = *GImGui;
//    const ImGuiStyle& style = g.Style;
//    const ImGuiID id = window->GetID(label);
//    const float w = CalcItemWidth();
//
//    const ImVec2 label_size = CalcTextSize(label, NULL, true);
//    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y*2.0f));
//    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));
//
//    ItemSize(total_bb, style.FramePadding.y);
//	if (!ItemAdd(total_bb, &id))
//    {
//        return false;
//    }
//	
//    bool hovered, held;
//    ButtonBehavior(frame_bb, id, &hovered, &held);
//    const ImU32 col = GetColorU32((hovered && held) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : path->empty() ? ImGuiCol_Column : ImGuiCol_Button);	
//    RenderFrame( frame_bb.Min, frame_bb.Max, col, true, style.FrameRounding );
//	
//	fs::path filename = path->has_filename() ? path->filename() : path->has_stem() ? path->stem() : *path;
//	fs::path dir = path->has_parent_path() && path->parent_path().has_stem() ? path->parent_path().stem() : fs::path();
//    RenderTextClipped( frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding, ( dir / filename ).string().c_str(), NULL, NULL, ImVec2(0.5f,0.5f) );
//	
//	bool changed = false;
//	PushID( label );
//	if( BeginPopupContextItem( "##FilePickerContextPopup", 0 ) ) {
//		if( MenuItem( string( "Load " + string( label ) ).c_str(), "", nullptr ) ) {
//			auto newPath = open ? getOpenFilePath( initialPath, extensions ) : getSaveFilePath( initialPath, extensions );
//			if( ! newPath.empty() ) {
//				*path = newPath;
//				changed = true;
//			}
//		}
//		if( MenuItem( "Clear", "", nullptr ) ) {
//			path->clear();
//			changed = true;
//		}
//		EndPopup();
//	}
//	PopID();
//
//	if( hovered && ! path->empty() ) {
//		BeginTooltip();
//		TextUnformatted( path->string().c_str() );
//		EndTooltip();
//	}
//	
//
//    if (label_size.x > 0.0f)
//        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);
//
//	return changed;
//
//}
//
//bool IconButton( const char* icon, const ImVec2& size_arg, bool frame )
//{
//    ImGuiWindow* window = GetCurrentWindow();
//    if (window->SkipItems)
//        return false;
//
//    ImGuiContext& g = *GImGui;
//    const ImGuiStyle& style = g.Style;
//    const ImGuiID id = window->GetID(icon);
//    const ImVec2 label_size = CalcTextSize(icon, NULL, true);
//	
//	ImGuiButtonFlags flags = 0;
//    ImVec2 pos = window->DC.CursorPos;
//    if ((flags & ImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y < window->DC.CurrentLineTextBaseOffset) // Try to vertically align buttons that are smaller/have no padding so that text baseline matches (bit hacky, since it shouldn't be a flag)
//        pos.y += window->DC.CurrentLineTextBaseOffset - style.FramePadding.y;
//    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);
//
//    const ImRect bb(pos, pos + size);
//    ItemSize(bb, style.FramePadding.y);
//    if (!ItemAdd(bb, &id))
//        return false;
//
//    if (window->DC.ItemFlags & ImGuiItemFlags_ButtonRepeat) flags |= ImGuiButtonFlags_Repeat;
//
//    bool hovered, held;
//    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);
//
//    // Render
//	const ImU32 col = GetColorU32((hovered && held) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Text);
//	ScopedStyleColor color( ImGuiCol_Text, ImVec4( (float)((col >> 0) & 0xFF) / 255.0f, (float)((col >> 8) & 0xFF) / 255.0f, (float)((col >> 16) & 0xFF) / 255.0f, (float)((col >> 24) & 0xFF) / 255.0f ) );
//    RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, icon, NULL, &label_size, style.ButtonTextAlign, &bb);
//
//	return pressed;
//}
//
//bool IconToggle( const char* iconEnabled, const char* iconDisabled, bool *enabled, const ImVec2& size, bool frame )
//{
//    ImGuiWindow* window = GetCurrentWindow();
//    if (window->SkipItems)
//        return false;
//
//	bool changed = IconButton( *enabled ? iconEnabled : iconDisabled, size, frame );
//	if( changed ) {
//		*enabled = ! *enabled;
//	}
//	return changed;
//}

#if defined( IMGUI_DOCK )

struct DockContext
{
	enum Slot_
	{
		Slot_Left,
		Slot_Right,
		Slot_Top,
		Slot_Bottom,
		Slot_Tab,

		Slot_Float,
		Slot_None
	};


	enum EndAction_
	{
		EndAction_None,
		EndAction_Panel,
		EndAction_End,
		EndAction_EndChild
	};


	enum Status_
	{
		Status_Docked,
		Status_Float,
		Status_Dragged
	};


	struct Dock
	{
		Dock()
			: id(0)
			, next_tab(nullptr)
			, prev_tab(nullptr)
			, parent(nullptr)
			, pos(0, 0)
			, size(-1, -1)
			, active(true)
			, status(Status_Float)
			, label(nullptr)
			, opened(false)
		{
			location[0] = 0;
			children[0] = children[1] = nullptr;
		}


		~Dock() { MemFree(label); }


		ImVec2 getMinSize() const
		{
			if (!children[0]) return ImVec2(16, 16 + GetTextLineHeightWithSpacing());

			ImVec2 s0 = children[0]->getMinSize();
			ImVec2 s1 = children[1]->getMinSize();
			return isHorizontal() ? ImVec2(s0.x + s1.x, ImMax(s0.y, s1.y))
								  : ImVec2(ImMax(s0.x, s1.x), s0.y + s1.y);
		}


		bool isHorizontal() const { return children[0]->pos.x < children[1]->pos.x; }


		void setParent(Dock* dock)
		{
			parent = dock;
			for (Dock* tmp = prev_tab; tmp; tmp = tmp->prev_tab) tmp->parent = dock;
			for (Dock* tmp = next_tab; tmp; tmp = tmp->next_tab) tmp->parent = dock;
		}


		Dock& getSibling()
		{
			IM_ASSERT(parent);
			if (parent->children[0] == &getFirstTab()) return *parent->children[1];
			return *parent->children[0];
		}


		Dock& getFirstTab()
		{
			Dock* tmp = this;
			while (tmp->prev_tab) tmp = tmp->prev_tab;
			return *tmp;
		}


		void setActive()
		{
			active = true;
			for (Dock* tmp = prev_tab; tmp; tmp = tmp->prev_tab) tmp->active = false;
			for (Dock* tmp = next_tab; tmp; tmp = tmp->next_tab) tmp->active = false;
		}


		bool isContainer() const { return children[0] != nullptr; }


		void setChildrenPosSize(const ImVec2& _pos, const ImVec2& _size)
		{
			ImVec2 s = children[0]->size;
			if (isHorizontal())
			{
				s.y = _size.y;
				s.x = (float)int(
					_size.x * children[0]->size.x / (children[0]->size.x + children[1]->size.x));
				if (s.x < children[0]->getMinSize().x)
				{
					s.x = children[0]->getMinSize().x;
				}
				else if (_size.x - s.x < children[1]->getMinSize().x)
				{
					s.x = _size.x - children[1]->getMinSize().x;
				}
				children[0]->setPosSize(_pos, s);

				s.x = _size.x - children[0]->size.x;
				ImVec2 p = _pos;
				p.x += children[0]->size.x;
				children[1]->setPosSize(p, s);
			}
			else
			{
				s.x = _size.x;
				s.y = (float)int(
					_size.y * children[0]->size.y / (children[0]->size.y + children[1]->size.y));
				if (s.y < children[0]->getMinSize().y)
				{
					s.y = children[0]->getMinSize().y;
				}
				else if (_size.y - s.y < children[1]->getMinSize().y)
				{
					s.y = _size.y - children[1]->getMinSize().y;
				}
				children[0]->setPosSize(_pos, s);

				s.y = _size.y - children[0]->size.y;
				ImVec2 p = _pos;
				p.y += children[0]->size.y;
				children[1]->setPosSize(p, s);
			}
		}


		void setPosSize(const ImVec2& _pos, const ImVec2& _size)
		{
			size = _size;
			pos = _pos;
			for (Dock* tmp = prev_tab; tmp; tmp = tmp->prev_tab)
			{
				tmp->size = _size;
				tmp->pos = _pos;
			}
			for (Dock* tmp = next_tab; tmp; tmp = tmp->next_tab)
			{
				tmp->size = _size;
				tmp->pos = _pos;
			}

			if (!isContainer()) return;
			setChildrenPosSize(_pos, _size);
		}


		char* label;
		ImU32 id;
		Dock* next_tab;
		Dock* prev_tab;
		Dock* children[2];
		Dock* parent;
		bool active;
		ImVec2 pos;
		ImVec2 size;
		Status_ status;
		int last_frame;
		int invalid_frames;
		char location[16];
		bool opened;
		bool first;
	};


	ImVector<Dock*> m_docks;
	ImVec2 m_drag_offset;
	Dock* m_current = nullptr;
	int m_last_frame = 0;
	EndAction_ m_end_action;


	~DockContext() {}


	Dock& getDock(const char* label, bool opened)
	{
		ImU32 id = ImHash(label, 0);
		for (int i = 0; i < m_docks.size(); ++i)
		{
			if (m_docks[i]->id == id) return *m_docks[i];
		}

		Dock* new_dock = (Dock*)MemAlloc(sizeof(Dock));
		IM_PLACEMENT_NEW(new_dock) Dock();
		m_docks.push_back(new_dock);
		new_dock->label = ImStrdup(label);
		IM_ASSERT(new_dock->label);
		new_dock->id = id;
		new_dock->setActive();
		new_dock->status = Status_Float;
		new_dock->pos = ImVec2(0, 0);
		new_dock->size = GetIO().DisplaySize;
		new_dock->opened = opened;
		new_dock->first = true;
		new_dock->last_frame = 0;
		new_dock->invalid_frames = 0;
		new_dock->location[0] = 0;
		return *new_dock;
	}


	void putInBackground()
	{
		ImGuiWindow* win = GetCurrentWindow();
		ImGuiContext& g = *GImGui;
		if (g.Windows[0] == win) return;

		for (int i = 0; i < g.Windows.Size; i++)
		{
			if (g.Windows[i] == win)
			{
				for (int j = i - 1; j >= 0; --j)
				{
					g.Windows[j + 1] = g.Windows[j];
				}
				g.Windows[0] = win;
				break;
			}
		}
	}


	void splits()
	{
		if (GetFrameCount() == m_last_frame) return;
		m_last_frame = GetFrameCount();

		putInBackground();

		ImU32 color = GetColorU32(ImGuiCol_Button);
		ImU32 color_hovered = GetColorU32(ImGuiCol_ButtonHovered);
		ImDrawList* draw_list = GetWindowDrawList();
		ImGuiIO& io = GetIO();
		for (int i = 0; i < m_docks.size(); ++i)
		{
			Dock& dock = *m_docks[i];
			if (!dock.isContainer()) continue;

			PushID(i);
			if (!IsMouseDown(0)) dock.status = Status_Docked;

			ImVec2 size = dock.children[0]->size;
			ImVec2 dsize(0, 0);
			SetCursorScreenPos(dock.children[1]->pos);
			ImVec2 min_size0 = dock.children[0]->getMinSize();
			ImVec2 min_size1 = dock.children[1]->getMinSize();
			if (dock.isHorizontal())
			{
				InvisibleButton("split", ImVec2(3, dock.size.y));
				if (dock.status == Status_Dragged) dsize.x = io.MouseDelta.x;
				dsize.x = -ImMin(-dsize.x, dock.children[0]->size.x - min_size0.x);
				dsize.x = ImMin(dsize.x, dock.children[1]->size.x - min_size1.x);
			}
			else
			{
				InvisibleButton("split", ImVec2(dock.size.x, 3));
				if (dock.status == Status_Dragged) dsize.y = io.MouseDelta.y;
				dsize.y = -ImMin(-dsize.y, dock.children[0]->size.y - min_size0.y);
				dsize.y = ImMin(dsize.y, dock.children[1]->size.y - min_size1.y);
			}
			ImVec2 new_size0 = dock.children[0]->size + dsize;
			ImVec2 new_size1 = dock.children[1]->size - dsize;
			ImVec2 new_pos1 = dock.children[1]->pos + dsize;
			dock.children[0]->setPosSize(dock.children[0]->pos, new_size0);
			dock.children[1]->setPosSize(new_pos1, new_size1);

			if (IsItemHovered() && IsMouseClicked(0))
			{
				dock.status = Status_Dragged;
			}

			draw_list->AddRectFilled(
				GetItemRectMin(), GetItemRectMax(), IsItemHovered() ? color_hovered : color);
			PopID();
		}
	}


	void checkNonexistent()
	{
		int frame_limit = ImMax(0, ImGui::GetFrameCount() - 2);
		for (Dock* dock : m_docks)
		{
			if (dock->isContainer()) continue;
			if (dock->status == Status_Float) continue;
			if (dock->last_frame < frame_limit)
			{
				++dock->invalid_frames;
				if (dock->invalid_frames > 2)
				{
					doUndock(*dock);
					dock->status = Status_Float;
				}
				return;
			}
			dock->invalid_frames = 0;
		}
	}


	void beginPanel()
	{
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
								 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
								 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
								 ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_ShowBorders | 
								 ImGuiWindowFlags_NoBringToFrontOnFocus;
		Dock* root = getRootDock();
		if (root)
		{
			SetNextWindowPos(root->pos);
			SetNextWindowSize(root->size);
		}
		else
		{
			SetNextWindowPos(ImVec2(0, 0));
			SetNextWindowSize(GetIO().DisplaySize);
		}

		//ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
		Begin("###DockPanel", nullptr, flags);
		splits();

		checkNonexistent();
	}


	void endPanel()
	{
		End();
		//ImGui::PopStyleVar();
	}


	Dock* getDockAt(const ImVec2& pos) const
	{
		for (int i = 0; i < m_docks.size(); ++i)
		{
			Dock& dock = *m_docks[i];
			if (dock.isContainer()) continue;
			if (dock.status != Status_Docked) continue;
			if (IsMouseHoveringRect(dock.pos, dock.pos + dock.size, false))
			{
				return &dock;
			}
		}

		return nullptr;
	}


	static ImRect getDockedRect(const ImRect& rect, Slot_ dock_slot)
	{
		ImVec2 half_size = rect.GetSize() * 0.5f;
		switch (dock_slot)
		{
			default: return rect;
			case Slot_Top: return ImRect(rect.Min, rect.Min + ImVec2(rect.Max.x, half_size.y));
			case Slot_Right: return ImRect(rect.Min + ImVec2(half_size.x, 0), rect.Max);
			case Slot_Bottom: return ImRect(rect.Min + ImVec2(0, half_size.y), rect.Max);
			case Slot_Left: return ImRect(rect.Min, rect.Min + ImVec2(half_size.x, rect.Max.y));
		}
	}


	static ImRect getSlotRect(ImRect parent_rect, Slot_ dock_slot)
	{
		ImVec2 size = parent_rect.Max - parent_rect.Min;
		ImVec2 center = parent_rect.Min + size * 0.5f;
		switch (dock_slot)
		{
			default: return ImRect(center - ImVec2(20, 20), center + ImVec2(20, 20));
			case Slot_Top: return ImRect(center + ImVec2(-20, -50), center + ImVec2(20, -30));
			case Slot_Right: return ImRect(center + ImVec2(30, -20), center + ImVec2(50, 20));
			case Slot_Bottom: return ImRect(center + ImVec2(-20, +30), center + ImVec2(20, 50));
			case Slot_Left: return ImRect(center + ImVec2(-50, -20), center + ImVec2(-30, 20));
		}
	}


	static ImRect getSlotRectOnBorder(ImRect parent_rect, Slot_ dock_slot)
	{
		ImVec2 size = parent_rect.Max - parent_rect.Min;
		ImVec2 center = parent_rect.Min + size * 0.5f;
		switch (dock_slot)
		{
			case Slot_Top:
				return ImRect(ImVec2(center.x - 20, parent_rect.Min.y + 10),
					ImVec2(center.x + 20, parent_rect.Min.y + 30));
			case Slot_Left:
				return ImRect(ImVec2(parent_rect.Min.x + 10, center.y - 20),
					ImVec2(parent_rect.Min.x + 30, center.y + 20));
			case Slot_Bottom:
				return ImRect(ImVec2(center.x - 20, parent_rect.Max.y - 30),
					ImVec2(center.x + 20, parent_rect.Max.y - 10));
			case Slot_Right:
				return ImRect(ImVec2(parent_rect.Max.x - 30, center.y - 20),
					ImVec2(parent_rect.Max.x - 10, center.y + 20));
			default: IM_ASSERT(false);
		}
		IM_ASSERT(false);
		return ImRect();
	}


	Dock* getRootDock()
	{
		for (int i = 0; i < m_docks.size(); ++i)
		{
			if (!m_docks[i]->parent &&
				(m_docks[i]->status == Status_Docked || m_docks[i]->children[0]))
			{
				return m_docks[i];
			}
		}
		return nullptr;
	}


	bool dockSlots(Dock& dock, Dock* dest_dock, const ImRect& rect, bool on_border)
	{
		ImDrawList* canvas = GetWindowDrawList();
		ImU32 color = GetColorU32(ImGuiCol_Button);
		ImU32 color_hovered = GetColorU32(ImGuiCol_ButtonHovered);
		ImVec2 mouse_pos = GetIO().MousePos;
		for (int i = 0; i < (on_border ? 4 : 5); ++i)
		{
			ImRect r =
				on_border ? getSlotRectOnBorder(rect, (Slot_)i) : getSlotRect(rect, (Slot_)i);
			bool hovered = r.Contains(mouse_pos);
			canvas->AddRectFilled(r.Min, r.Max, hovered ? color_hovered : color);
			if (!hovered) continue;

			if (!IsMouseDown(0))
			{
				doDock(dock, dest_dock ? dest_dock : getRootDock(), (Slot_)i);
				return true;
			}
			ImRect docked_rect = getDockedRect(rect, (Slot_)i);
			canvas->AddRectFilled(docked_rect.Min, docked_rect.Max, GetColorU32(ImGuiCol_Button));
		}
		return false;
	}


	void handleDrag(Dock& dock)
	{
		Dock* dest_dock = getDockAt(GetIO().MousePos);

		Begin("##Overlay",
			NULL,
			ImVec2(0, 0),
			0.f,
			ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_AlwaysAutoResize);
		ImDrawList* canvas = GetWindowDrawList();

		canvas->PushClipRectFullScreen();

		ImU32 docked_color = GetColorU32(ImGuiCol_FrameBg);
		docked_color = (docked_color & 0x00ffFFFF) | 0x80000000;
		dock.pos = GetIO().MousePos - m_drag_offset;
		if (dest_dock)
		{
			if (dockSlots(dock,
					dest_dock,
					ImRect(dest_dock->pos, dest_dock->pos + dest_dock->size),
					false))
			{
				canvas->PopClipRect();
				End();
				return;
			}
		}
		if (dockSlots(dock, nullptr, ImRect(ImVec2(0, 0), GetIO().DisplaySize), true))
		{
			canvas->PopClipRect();
			End();
			return;
		}
		canvas->AddRectFilled(dock.pos, dock.pos + dock.size, docked_color);
		canvas->PopClipRect();

		if (!IsMouseDown(0))
		{
			dock.status = Status_Float;
			dock.location[0] = 0;
			dock.setActive();
		}

		End();
	}


	void fillLocation(Dock& dock)
	{
		if (dock.status == Status_Float) return;
		char* c = dock.location;
		Dock* tmp = &dock;
		while (tmp->parent)
		{
			*c = getLocationCode(tmp);
			tmp = tmp->parent;
			++c;
		}
		*c = 0;
	}


	void doUndock(Dock& dock)
	{
		if (dock.prev_tab)
			dock.prev_tab->setActive();
		else if (dock.next_tab)
			dock.next_tab->setActive();
		else
			dock.active = false;
		Dock* container = dock.parent;

		if (container)
		{
			Dock& sibling = dock.getSibling();
			if (container->children[0] == &dock)
			{
				container->children[0] = dock.next_tab;
			}
			else if (container->children[1] == &dock)
			{
				container->children[1] = dock.next_tab;
			}

			bool remove_container = !container->children[0] || !container->children[1];
			if (remove_container)
			{
				if (container->parent)
				{
					Dock*& child = container->parent->children[0] == container
									   ? container->parent->children[0]
									   : container->parent->children[1];
					child = &sibling;
					child->setPosSize(container->pos, container->size);
					child->setParent(container->parent);
				}
				else
				{
					if (container->children[0])
					{
						container->children[0]->setParent(nullptr);
						container->children[0]->setPosSize(container->pos, container->size);
					}
					if (container->children[1])
					{
						container->children[1]->setParent(nullptr);
						container->children[1]->setPosSize(container->pos, container->size);
					}
				}
				for (int i = 0; i < m_docks.size(); ++i)
				{
					if (m_docks[i] == container)
					{
						m_docks.erase(m_docks.begin() + i);
						break;
					}
				}
				container->~Dock();
				MemFree(container);
			}
		}
		if (dock.prev_tab) dock.prev_tab->next_tab = dock.next_tab;
		if (dock.next_tab) dock.next_tab->prev_tab = dock.prev_tab;
		dock.parent = nullptr;
		dock.prev_tab = dock.next_tab = nullptr;
	}


	void drawTabbarListButton(Dock& dock)
	{
		if (!dock.next_tab) return;

		ImDrawList* draw_list = GetWindowDrawList();
		if (InvisibleButton("list", ImVec2(16, 16)))
		{
			OpenPopup("tab_list_popup");
		}
		if (BeginPopup("tab_list_popup"))
		{
			Dock* tmp = &dock;
			while (tmp)
			{
				bool dummy = false;
				if (Selectable(tmp->label, &dummy))
				{
					tmp->setActive();
				}
				tmp = tmp->next_tab;
			}
			EndPopup();
		}

		bool hovered = IsItemHovered();
		ImVec2 min = GetItemRectMin();
		ImVec2 max = GetItemRectMax();
		ImVec2 center = (min + max) * 0.5f;
		ImU32 text_color = GetColorU32(ImGuiCol_Text);
		ImU32 color_active = GetColorU32(ImGuiCol_FrameBgActive);
		draw_list->AddRectFilled(ImVec2(center.x - 4, min.y + 3),
			ImVec2(center.x + 4, min.y + 5),
			hovered ? color_active : text_color);
		draw_list->AddTriangleFilled(ImVec2(center.x - 4, min.y + 7),
			ImVec2(center.x + 4, min.y + 7),
			ImVec2(center.x, min.y + 12),
			hovered ? color_active : text_color);
	}


	bool tabbar(Dock& dock, bool close_button)
	{
		float tabbar_height = 2 * GetTextLineHeightWithSpacing();
		ImVec2 size(dock.size.x, tabbar_height);
		bool tab_closed = false;

		SetCursorScreenPos(dock.pos);
		char tmp[20];
		ImFormatString(tmp, IM_ARRAYSIZE(tmp), "tabs%d", (int)dock.id);
		if (BeginChild(tmp, size, true))
		{
			Dock* dock_tab = &dock;

			ImDrawList* draw_list = GetWindowDrawList();
			ImU32 color = GetColorU32(ImGuiCol_WindowBg);//ImGuiCol_FrameBg);
			ImU32 color_active = GetColorU32(ImGuiCol_WindowBg);//ImGuiCol_FrameBgActive);
			ImU32 color_hovered = GetColorU32(ImGuiCol_FrameBgActive);//ImGuiCol_FrameBgHovered);
			ImU32 text_color = GetColorU32(ImGuiCol_Text);
			float line_height = GetTextLineHeightWithSpacing();
			float tab_base;

			drawTabbarListButton(dock);

			while (dock_tab)
			{
				SameLine(0, 15);

				const char* text_end = FindRenderedTextEnd(dock_tab->label);
				ImVec2 size(CalcTextSize(dock_tab->label, text_end).x, line_height);
				if (InvisibleButton(dock_tab->label, size))
				{
					dock_tab->setActive();
				}

				if (IsItemActive() && IsMouseDragging())
				{
					m_drag_offset = GetMousePos() - dock_tab->pos;
					doUndock(*dock_tab);
					dock_tab->status = Status_Dragged;
				}

				bool hovered = IsItemHovered();
				ImVec2 pos = GetItemRectMin();
				if (dock_tab->active && close_button)
				{
					size.x += 16 + GetStyle().ItemSpacing.x;
					SameLine();
					tab_closed = InvisibleButton("close", ImVec2(16, 16));
					ImVec2 center = (GetItemRectMin() + GetItemRectMax()) * 0.5f;
					draw_list->AddLine(
						center + ImVec2(-3.5f, -3.5f), center + ImVec2(3.5f, 3.5f), text_color);
					draw_list->AddLine(
						center + ImVec2(3.5f, -3.5f), center + ImVec2(-3.5f, 3.5f), text_color);
				}
				tab_base = pos.y;
				ImGuiStyle &style = GetStyle();

				draw_list->PathClear();
				draw_list->AddRectFilled( pos + ImVec2( -5, 0 ), pos + ImVec2( size.x + 5, size.y ), hovered ? color_hovered : (dock_tab->active ? color_active : color), style.FrameRounding, 1 | 2 );
				draw_list->AddText( pos, text_color, dock_tab->label, text_end);

				dock_tab = dock_tab->next_tab;
			}
			ImVec2 cp(dock.pos.x, tab_base + line_height);
			draw_list->AddLine(cp, cp + ImVec2(dock.size.x, 0), color);
		}
		EndChild();
		return tab_closed;
	}


	static void setDockPosSize(Dock& dest, Dock& dock, Slot_ dock_slot, Dock& container)
	{
		IM_ASSERT(!dock.prev_tab && !dock.next_tab && !dock.children[0] && !dock.children[1]);

		dest.pos = container.pos;
		dest.size = container.size;
		dock.pos = container.pos;
		dock.size = container.size;

		switch (dock_slot)
		{
			case Slot_Bottom:
				dest.size.y *= 0.5f;
				dock.size.y *= 0.5f;
				dock.pos.y += dest.size.y;
				break;
			case Slot_Right:
				dest.size.x *= 0.5f;
				dock.size.x *= 0.5f;
				dock.pos.x += dest.size.x;
				break;
			case Slot_Left:
				dest.size.x *= 0.5f;
				dock.size.x *= 0.5f;
				dest.pos.x += dock.size.x;
				break;
			case Slot_Top:
				dest.size.y *= 0.5f;
				dock.size.y *= 0.5f;
				dest.pos.y += dock.size.y;
				break;
			default: IM_ASSERT(false); break;
		}
		dest.setPosSize(dest.pos, dest.size);

		if (container.children[1]->pos.x < container.children[0]->pos.x ||
			container.children[1]->pos.y < container.children[0]->pos.y)
		{
			Dock* tmp = container.children[0];
			container.children[0] = container.children[1];
			container.children[1] = tmp;
		}
	}


	void doDock(Dock& dock, Dock* dest, Slot_ dock_slot)
	{
		IM_ASSERT(!dock.parent);
		if (!dest)
		{
			dock.status = Status_Docked;
			dock.setPosSize(ImVec2(0, 0), GetIO().DisplaySize);
		}
		else if (dock_slot == Slot_Tab)
		{
			Dock* tmp = dest;
			while (tmp->next_tab)
			{
				tmp = tmp->next_tab;
			}

			tmp->next_tab = &dock;
			dock.prev_tab = tmp;
			dock.size = tmp->size;
			dock.pos = tmp->pos;
			dock.parent = dest->parent;
			dock.status = Status_Docked;
		}
		else if (dock_slot == Slot_None)
		{
			dock.status = Status_Float;
		}
		else
		{
			Dock* container = (Dock*)MemAlloc(sizeof(Dock));
			IM_PLACEMENT_NEW(container) Dock();
			m_docks.push_back(container);
			container->children[0] = &dest->getFirstTab();
			container->children[1] = &dock;
			container->next_tab = nullptr;
			container->prev_tab = nullptr;
			container->parent = dest->parent;
			container->size = dest->size;
			container->pos = dest->pos;
			container->status = Status_Docked;
			container->label = ImStrdup("");

			if (!dest->parent)
			{
			}
			else if (&dest->getFirstTab() == dest->parent->children[0])
			{
				dest->parent->children[0] = container;
			}
			else
			{
				dest->parent->children[1] = container;
			}

			dest->setParent(container);
			dock.parent = container;
			dock.status = Status_Docked;

			setDockPosSize(*dest, dock, dock_slot, *container);
		}
		dock.setActive();
	}


	void rootDock(const ImVec2& pos, const ImVec2& size)
	{
		Dock* root = getRootDock();
		if (!root) return;

		ImVec2 min_size = root->getMinSize();
		ImVec2 requested_size = size;
		root->setPosSize(pos, ImMax(min_size, requested_size));
	}


	void setDockActive()
	{
		IM_ASSERT(m_current);
		if (m_current) m_current->setActive();
	}


	static Slot_ getSlotFromLocationCode(char code)
	{
		switch (code)
		{
			case '1': return Slot_Left;
			case '2': return Slot_Top;
			case '3': return Slot_Bottom;
			default: return Slot_Right;
		}
	}


	static char getLocationCode(Dock* dock)
	{
		if (!dock) return '0';

		if (dock->parent->isHorizontal())
		{
			if (dock->pos.x < dock->parent->children[0]->pos.x) return '1';
			if (dock->pos.x < dock->parent->children[1]->pos.x) return '1';
			return '0';
		}
		else
		{
			if (dock->pos.y < dock->parent->children[0]->pos.y) return '2';
			if (dock->pos.y < dock->parent->children[1]->pos.y) return '2';
			return '3';
		}
	}


	void tryDockToStoredLocation(Dock& dock)
	{
		if (dock.status == Status_Docked) return;
		if (dock.location[0] == 0) return;
		
		Dock* tmp = getRootDock();
		if (!tmp) return;

		Dock* prev = nullptr;
		char* c = dock.location + strlen(dock.location) - 1;
		while (c >= dock.location && tmp)
		{
			prev = tmp;
			tmp = *c == getLocationCode(tmp->children[0]) ? tmp->children[0] : tmp->children[1];
			if(tmp) --c;
		}
		if (tmp && tmp->children[0]) tmp = tmp->parent;
		doDock(dock, tmp ? tmp : prev, tmp && !tmp->children[0] ? Slot_Tab : getSlotFromLocationCode(*c));
	}


	bool begin(const char* label, bool* opened, ImGuiWindowFlags extra_flags)
	{
		Dock& dock = getDock(label, !opened || *opened);
		if (!dock.opened && (!opened || *opened)) tryDockToStoredLocation(dock);
		dock.last_frame = ImGui::GetFrameCount();
		if (strcmp(dock.label, label) != 0)
		{
			MemFree(dock.label);
			dock.label = ImStrdup(label);
		}
		
		m_end_action = EndAction_None;
		if( dock.first ) {
			doDock( dock, getRootDock(), Slot_Tab );
		}
		if (dock.first && opened) *opened = dock.opened;
		dock.first = false;
		if (opened && !*opened)
		{
			if (dock.status != Status_Float)
			{
				fillLocation(dock);
				doUndock(dock);
				dock.status = Status_Float;
			}
			dock.opened = false;
			return false;
		}
		dock.opened = true;

		m_end_action = EndAction_Panel;
		beginPanel();

		m_current = &dock;
		if (dock.status == Status_Dragged) handleDrag(dock);

		bool is_float = dock.status == Status_Float;

		if (is_float)
		{
			SetNextWindowPos(dock.pos);
			SetNextWindowSize(dock.size);
			bool ret = Begin(label,
				opened,
				dock.size,
				-1.0f,
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_ShowBorders | extra_flags);
			m_end_action = EndAction_End;
			dock.pos = GetWindowPos();
			dock.size = GetWindowSize();

			ImGuiContext& g = *GImGui;

			if (g.ActiveId == GetCurrentWindow()->MoveId && g.IO.MouseDown[0])
			{
				m_drag_offset = GetMousePos() - dock.pos;
				doUndock(dock);
				dock.status = Status_Dragged;
			}
			return ret;
		}

		if (!dock.active && dock.status != Status_Dragged) return false;

		m_end_action = EndAction_EndChild;

		PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
		float tabbar_height = GetTextLineHeightWithSpacing();
		if (tabbar(dock.getFirstTab(), opened != nullptr))
		{
			fillLocation(dock);
			*opened = false;
		}
		ImVec2 pos = dock.pos;
		ImVec2 size = dock.size;
		pos.y += tabbar_height + GetStyle().WindowPadding.y;
		size.y -= tabbar_height + GetStyle().WindowPadding.y;

		SetCursorScreenPos(pos);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
								 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
								 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
								 extra_flags;
		char tmp[256];
		strcpy(tmp, label);
		strcat(tmp, "_docked"); // to avoid https://github.com/ocornut/imgui/issues/713
		bool ret = BeginChild(tmp, size, true, flags);
		PopStyleColor();
		PopStyleColor();
		return ret;
	}


	void end()
	{
		if (m_end_action == EndAction_End)
		{
			End();
		}
		else if (m_end_action == EndAction_EndChild)
		{
			PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
			PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
			EndChild();
			PopStyleColor();
			PopStyleColor();
		}
		m_current = nullptr;
		if (m_end_action > EndAction_None) endPanel();
	}


	int getDockIndex(Dock* dock)
	{
		if (!dock) return -1;

		for (int i = 0; i < m_docks.size(); ++i)
		{
			if (dock == m_docks[i]) return i;
		}

		IM_ASSERT(false);
		return -1;
	}

	/*
	void save(Lumix::FS::OsFile& file)
	{
		file << "docks = {\n";
		for (int i = 0; i < m_docks.size(); ++i)
		{
			Dock& dock = *m_docks[i];
			file << "dock" << (Lumix::uint64)&dock << " = {\n";
			file << "index = " << i << ",\n";
			file << "label = \"" << dock.label << "\",\n";
			file << "x = " << (int)dock.pos.x << ",\n";
			file << "y = " << (int)dock.pos.y << ",\n";
			file << "location = \"" << dock.location << "\",\n";
			file << "size_x = " << (int)dock.size.x << ",\n";
			file << "size_y = " << (int)dock.size.y << ",\n";
			file << "status = " << (int)dock.status << ",\n";
			file << "active = " << (int)dock.active << ",\n";
			file << "opened = " << (int)dock.opened << ",\n";
			file << "prev = " << (int)getDockIndex(dock.prev_tab) << ",\n";
			file << "next = " << (int)getDockIndex(dock.next_tab) << ",\n";
			file << "child0 = " << (int)getDockIndex(dock.children[0]) << ",\n";
			file << "child1 = " << (int)getDockIndex(dock.children[1]) << ",\n";
			file << "parent = " << (int)getDockIndex(dock.parent) << "\n";
			if (i < m_docks.size() - 1)
				file << "},\n";
			else
				file << "}\n";
		}
		file << "}\n";
	}
	

	Dock* getDockByIndex(lua_Integer idx) { return idx < 0 ? nullptr : m_docks[(int)idx]; }


	void load(lua_State* L)
	{
		for (int i = 0; i < m_docks.size(); ++i)
		{
			m_docks[i]->~Dock();
			MemFree(m_docks[i]);
		}
		m_docks.clear();

		if (lua_getglobal(L, "docks") == LUA_TTABLE)
		{
			lua_pushnil(L);
			while (lua_next(L, -2) != 0)
			{
				Dock* new_dock = (Dock*)MemAlloc(sizeof(Dock));
				m_docks.push_back(IM_PLACEMENT_NEW(new_dock) Dock());
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);

		if (lua_getglobal(L, "docks") == LUA_TTABLE)
		{
			lua_pushnil(L);
			while (lua_next(L, -2) != 0)
			{
				if (lua_istable(L, -1))
				{
					int idx = 0;
					if (lua_getfield(L, -1, "index") == LUA_TNUMBER)
						idx = (int)lua_tointeger(L, -1);
					Dock& dock = *m_docks[idx];
					dock.last_frame = 0;
					dock.invalid_frames = 0;
					lua_pop(L, 1);

					if (lua_getfield(L, -1, "label") == LUA_TSTRING)
					{
						dock.label = ImStrdup(lua_tostring(L, -1));
						dock.id = ImHash(dock.label, 0);
					}
					lua_pop(L, 1);

					if (lua_getfield(L, -1, "x") == LUA_TNUMBER)
						dock.pos.x = (float)lua_tonumber(L, -1);
					if (lua_getfield(L, -2, "y") == LUA_TNUMBER)
						dock.pos.y = (float)lua_tonumber(L, -1);
					if (lua_getfield(L, -3, "size_x") == LUA_TNUMBER)
						dock.size.x = (float)lua_tonumber(L, -1);
					if (lua_getfield(L, -4, "size_y") == LUA_TNUMBER)
						dock.size.y = (float)lua_tonumber(L, -1);
					if (lua_getfield(L, -5, "active") == LUA_TNUMBER)
						dock.active = lua_tointeger(L, -1) != 0;
					if (lua_getfield(L, -6, "opened") == LUA_TNUMBER)
						dock.opened = lua_tointeger(L, -1) != 0;
					if (lua_getfield(L, -7, "location") == LUA_TSTRING)
						strcpy(dock.location, lua_tostring(L, -1));
					if (lua_getfield(L, -8, "status") == LUA_TNUMBER)
					{
						dock.status = (Status_)lua_tointeger(L, -1);
					}
					lua_pop(L, 8);

					if (lua_getfield(L, -1, "prev") == LUA_TNUMBER)
					{
						dock.prev_tab = getDockByIndex(lua_tointeger(L, -1));
					}
					if (lua_getfield(L, -2, "next") == LUA_TNUMBER)
					{
						dock.next_tab = getDockByIndex(lua_tointeger(L, -1));
					}
					if (lua_getfield(L, -3, "child0") == LUA_TNUMBER)
					{
						dock.children[0] = getDockByIndex(lua_tointeger(L, -1));
					}
					if (lua_getfield(L, -4, "child1") == LUA_TNUMBER)
					{
						dock.children[1] = getDockByIndex(lua_tointeger(L, -1));
					}
					if (lua_getfield(L, -5, "parent") == LUA_TNUMBER)
					{
						dock.parent = getDockByIndex(lua_tointeger(L, -1));
					}
					lua_pop(L, 5);
				}
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);
	}*/
};


static DockContext g_dock;


void ShutdownDock()
{
	for (int i = 0; i < g_dock.m_docks.size(); ++i)
	{
		g_dock.m_docks[i]->~Dock();
		MemFree(g_dock.m_docks[i]);
	}
	g_dock.m_docks.clear();
}


void RootDock(const ImVec2& pos, const ImVec2& size)
{
	g_dock.rootDock(pos, size);
}


void SetDockActive()
{
	g_dock.setDockActive();
}


bool BeginDock(const char* label, bool* opened, ImGuiWindowFlags extra_flags)
{
	return g_dock.begin(label, opened, extra_flags);
}


void EndDock()
{
	g_dock.end();
}

#endif

}
