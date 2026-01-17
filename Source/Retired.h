// Copyright Seong Woo Lee. All Rights Reserved.

static u8 *DebugCreateTiledBitmap(u32 Width, u32 Height, u32 TileSize, UINT8 R, UINT8 G, UINT8 B)
{
    u32 Stride = Width*4;
    u8 *Data = new u8[Stride*Height];
    memset(Data, 0, Stride*Height);
    for (u32 Y = 0; Y < Height; ++Y) 
    {
        for (u32 X = 0; X < Width; ++X) 
        {
            if ((X/TileSize + Y/TileSize)%2 == 0)
            {
                u32 *Texel = (u32 *)Data + Width*Y + X;
                *Texel = (UINT32)R | ((UINT32)G<<8) | ((UINT32)B<<16) | 0xff000000; 
            }
        }
    }
    return Data;
}


    // @Temporary
    vertex Vertices[] = {
        { Vec3(-0.25f, -0.25f, 0.0f), Vec4(1.0f, 1.0f, 1.0f, 1.0f), Vec2(0.f, 1.f) },
        { Vec3( 0.25f, -0.25f, 0.0f), Vec4(1.0f, 1.0f, 1.0f, 1.0f), Vec2(1.f, 1.f) },
        { Vec3(-0.25f,  0.25f, 0.0f), Vec4(1.0f, 1.0f, 1.0f, 1.0f), Vec2(0.f, 0.f) },
        { Vec3( 0.25f,  0.25f, 0.0f), Vec4(1.0f, 1.0f, 1.0f, 1.0f), Vec2(1.f, 0.f) },
    };
    u32 VertexCount = _countof(Vertices);
    u16 Indices[] = { 0, 2, 1, 1, 2, 3 };
    UINT IndexCount = _countof(Indices);


