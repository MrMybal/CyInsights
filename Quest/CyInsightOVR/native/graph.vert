// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#version 450
layout(location=0) in vec2 position;
layout(location=1) in vec4 tint;
layout(location=0) out vec4 color;
void main(){gl_Position=vec4(position,0,1);color=tint;}
