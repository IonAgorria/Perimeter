#ifndef PERIMETER_SOKOLRENDER_H
#define PERIMETER_SOKOLRENDER_H

#if defined(SOKOL_GLCORE33) || defined(SOKOL_GLES2) || defined(SOKOL_GLES3)
#define SOKOL_GL (1)
#endif

#include "sokol_gfx.h"
#include <SDL_video.h>
#ifdef SOKOL_METAL
#include <SDL_metal.h>
#endif
#include "SokolRenderPipeline.h"

const int PERIMETER_SOKOL_TEXTURES = 2;

#ifdef SOKOL_METAL
void sokol_metal_setup_desc(SDL_MetalView view, sg_desc* desc);
#endif

struct SokolCommand {
    SokolCommand();
    ~SokolCommand();
    void Clear();
    void ClearDrawData();
    void ClearMVP();
    void SetTexture(size_t index, cTexture* texture, SokolTexture2D* sokol_texture);
    void ClearTextures();
    NO_COPY_CONSTRUCTOR(SokolCommand)
    
    pipeline_id_t pipeline_id = 0;
    size_t vertices = 0;
    size_t indices = 0;
    struct SokolTexture2D* sokol_textures[PERIMETER_SOKOL_TEXTURES] = {};
    class cTexture* texture_handles[PERIMETER_SOKOL_TEXTURES] = {};
    bool owned_vertex_buffer = false;
    bool owned_index_buffer = false;
    struct SokolBuffer* vertex_buffer = nullptr;
    struct SokolBuffer* index_buffer = nullptr;
    bool owned_mvp = false;
    Mat4f* vs_mvp = nullptr;
    eColorMode fs_color_mode = COLOR_MOD;
    float fs_tex2_lerp = -1;
    eAlphaTestMode fs_alpha_test = ALPHATEST_NONE;
};

class cSokolRender: public cInterfaceRenderDevice {
private:
    //SDL context
    SDL_Window* sdlWindow = nullptr;
#ifdef SOKOL_GL
    SDL_GLContext sdlGlContext = nullptr;
#endif
#ifdef SOKOL_METAL
    SDL_MetalView sdlMetalView = nullptr;
#endif
    
    //Renderer state
    bool ActiveScene = false;
    sColor4f fill_color;
    std::vector<SokolCommand*> commands;
    Vect2i viewportPos;
    Vect2i viewportSize;
    
    //Empty texture when texture slot is unused
    SokolTexture2D* emptyTexture = nullptr;

    //Pipelines
    std::unordered_map<std::string, sg_shader> shaders;
    std::unordered_map<pipeline_id_t, struct SokolPipeline*> pipelines;
    static pipeline_id_t GetPipelineID(PIPELINE_TYPE type, vertex_fmt_t vertex_fmt, const PIPELINE_MODE& mode);
    static void GetPipelineIDParts(pipeline_id_t id, PIPELINE_TYPE* type, vertex_fmt_t* vertex_fmt, PIPELINE_MODE* mode);
    void ClearPipelines();
    void RegisterPipeline(pipeline_id_t id);
    
    //Active pipeline/command state
    SokolCommand activeCommand;
    PIPELINE_TYPE activePipelineType = PIPELINE_TYPE_DEFAULT;
    PIPELINE_MODE activePipelineMode;
    Mat4f activeCommandVP;
    Mat4f activeCommandW;

    //Commands handling
    void ClearCommands();
    void FinishCommand();
    void SetVPMatrix(const Mat4f* matrix);
    void SetTex2Lerp(float lerp);
    void SetColorMode(eColorMode color_mode);
    void SetTextures(float Phase, cTexture* tex0, cTexture* tex1);

    //Updates internal state after init/resolution change
    int UpdateRenderMode();

public:
    cSokolRender();
    ~cSokolRender() override;

    // //// cInterfaceRenderDevice impls start ////

    eRenderDeviceSelection GetRenderSelection() const override;

    uint32_t GetWindowCreationFlags() const override;
    void SetActiveDrawBuffer(class DrawBuffer*) override;
    
    int Init(int xScr,int yScr,int mode,void *hWnd=0,int RefreshRateInHz=0) override;
    bool ChangeSize(int xScr,int yScr,int mode) override;

    int GetClipRect(int *xmin,int *ymin,int *xmax,int *ymax) override;
    int SetClipRect(int xmin,int ymin,int xmax,int ymax) override;
    
    int Done() override;

    void SetDrawNode(cCamera *pDrawNode) override;
    void SetWorldMat4f(const Mat4f* matrix) override;
    void UseOrthographicProjection() override;
    void SetDrawTransform(class cCamera *DrawNode) override;

    int BeginScene() override;
    int EndScene() override;
    int Fill(int r,int g,int b,int a=255) override;
    int Flush(bool wnd=false) override;
    
    int SetGamma(float fGamma,float fStart=0.f,float fFinish=1.f) override;

    void CreateVertexBuffer(class VertexBuffer &vb, uint32_t NumberVertex, vertex_fmt_t fmt, bool dynamic) override;
    void DeleteVertexBuffer(class VertexBuffer &vb) override;
    void* LockVertexBuffer(class VertexBuffer &vb) override;
    void* LockVertexBuffer(class VertexBuffer &vb, uint32_t Start, uint32_t Amount) override;
    void UnlockVertexBuffer(class VertexBuffer &vb) override;
    void CreateIndexBuffer(class IndexBuffer& ib, uint32_t NumberIndices, bool dynamic) override;
    void DeleteIndexBuffer(class IndexBuffer &ib) override;
    indices_t* LockIndexBuffer(class IndexBuffer &ib) override;
    indices_t* LockIndexBuffer(class IndexBuffer &ib, uint32_t Start, uint32_t Amount) override;
    void UnlockIndexBuffer(class IndexBuffer &ib) override;
    void SubmitDrawBuffer(class DrawBuffer* db) override;
    int CreateTexture(class cTexture *Texture,class cFileImage *FileImage,bool enable_assert=true) override;
    int DeleteTexture(class cTexture *Texture) override;
    void* LockTexture(class cTexture *Texture, int& Pitch) override;
    void UnlockTexture(class cTexture *Texture) override;

    void SetGlobalFog(const sColor4f &color,const Vect2f &v) override;

    void SetGlobalLight(Vect3f *vLight, sColor4f *Ambient = nullptr,
                        sColor4f *Diffuse = nullptr, sColor4f *Specular = nullptr) override;

    uint32_t GetRenderState(eRenderStateOption option) override;
    int SetRenderState(eRenderStateOption option,uint32_t value) override;

    void OutText(int x,int y,const char *string,const sColor4f& color,int align=-1,eBlendMode blend_mode=ALPHA_BLEND) override;
    void OutText(int x,int y,const char *string,const sColor4f& color,int align,eBlendMode blend_mode,
                 cTexture* pTexture,eColorMode mode,Vect2f uv,Vect2f duv,float phase=0,float lerp_factor=1) override;

    bool SetScreenShot(const char *fname) override;

    void DrawSprite2(int x,int y,int dx,int dy,float u,float v,float du,float dv,float u1,float v1,float du1,float dv1,
                     cTexture *Tex1,cTexture *Tex2,float lerp_factor,float alpha=1,float phase=0,eColorMode mode=COLOR_MOD,eBlendMode blend_mode=ALPHA_NONE) override;

    bool IsEnableSelfShadow() override;

    void SetNoMaterial(eBlendMode blend,float Phase=0,cTexture *Texture0=0,cTexture *Texture1=0,eColorMode color_mode=COLOR_MOD) override;
    void SetBlendState(eBlendMode blend) override;

    void BeginDrawMesh(bool obj_mesh, bool use_shadow) override;
    void EndDrawMesh() override;
    void SetSimplyMaterialMesh(cObjMesh* mesh, sDataRenderMaterial* data) override;
    void DrawNoMaterialMesh(cObjMesh* mesh, sDataRenderMaterial* data) override;

    void BeginDrawShadow(bool shadow_map) override;
    void EndDrawShadow() override;
    void SetSimplyMaterialShadow(cObjMesh* mesh, cTexture* texture) override;
    void DrawNoMaterialShadow(cObjMesh* mesh) override;

    // //// cInterfaceRenderDevice impls end ////
};

#endif //PERIMETER_SOKOLRENDER_H
