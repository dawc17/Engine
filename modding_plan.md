Plan: Modding support roadmap (draft)
Start with safe, data-only mods, then add limited logic hooks after save compatibility and registry stability are in place. This reduces crash/security risk, keeps worlds loadable across updates, and lets you ship modding incrementally. The first milestone is runtime manifests + asset packs; later milestones add deterministic events and sandboxed scripting. Draft for review.

Steps
Define mod.json schema and namespace rules (namespace:name) in README.md.
Add a registry layer for blocks/items/surfaces in BlockTypes.cpp and Inventory.cpp.
Persist mod list, registry hash, and schema version in saves via WorldSession.cpp and RegionManager.cpp.
Implement runtime asset-pack override (mods first, embedded fallback) in ShaderClass.cpp, AudioEngine.cpp, and ToolModelGenerator.cpp.
Add bounded event hooks (onBlockBreak, onBlockPlace, onChunkGeneratePost, onCommand) around WorldSession.cpp, TerrainGenerator.cpp, and main.cpp.
Introduce sandbox + compatibility gates (API semver, permissions, safe mode) in startup flow from main.cpp.
Further Considerations
Logic-mod runtime choice: Option A Lua (fast adoption) / Option B WASM (stronger isolation) / Option C native plugins (max power, highest risk).
Save compatibility policy: strict fail on mismatch vs migration tools for renamed/removed mod IDs.
If approved, this can be expanded into a concrete v1 spec with folder layout, manifest fields, and API surface.