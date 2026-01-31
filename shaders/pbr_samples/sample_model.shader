// Sample PBR shader showing the new keywords.
// Textures named with *_emissive, *_clearcoat, etc. will automatically be discovered too.
// When r_pbr_shExtract is enabled, shcoeffs can be omitted and will be sourced from irradiance cubemaps.
sample_model_shader
{
    qer_editorimage textures/pbr_samples/sample_base.tga
    surfaceparm trans

    stage
    {
        map textures/pbr_samples/sample_base.tga
        normalMap textures/pbr_samples/sample_normal.tga
        rmoMap textures/pbr_samples/sample_rmo.tga
        emissiveMap textures/pbr_samples/sample_emissive.tga
        clearcoatMap textures/pbr_samples/sample_clearcoat.tga
        sheenMap textures/pbr_samples/sample_sheen.tga
        anisotropyMap textures/pbr_samples/sample_aniso.tga
        transmissionMap textures/pbr_samples/sample_transmission.tga
        subsurfaceMap textures/pbr_samples/sample_subsurface.tga
        subsurfaceColor 0.25 0.35 0.45
        subsurfaceParams 0.6 0.2 0.0 0.0
        emissiveScale 1.0 1.0 1.0
        clearcoatScale 0.8 0.3
        sheenScale 0.4 0.4 0.4
        anisotropyScale 0.5 0.1
        transmissionScale 0.4 0.1 0.2
        shcoeffs
            0.45 0.46 0.48
            0.01 0.02 0.01
            0.00 0.03 0.02
            0.02 0.01 0.01
            0.00 0.00 0.00
            0.00 0.00 0.00
            0.05 0.05 0.05
            0.00 0.00 0.00
            0.00 0.00 0.00
    }
}
