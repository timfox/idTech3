# Deferred AO ownership

Material AO and screen-space/GTAO are indirect-light inputs. They do not darken
emissive or direct lighting and must not be applied again in the final
owner-based composite. Forward-owned opaque pixels consume the same current AO
resource through their Forward+ path. WBOIT retains its documented compatible
approximation.

AO temporal history is independent of the quarantined Temporal SSR history and
requires its own generation/reset validation. `ao_ownership_validate` remains
pending until current GPU evidence proves one application per pixel.
