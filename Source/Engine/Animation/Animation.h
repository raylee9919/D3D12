// Copyright Seong Woo Lee. All Rights Reserved.

#define MAX_JOINT_COUNT 128

struct joint
{
    std::string Name;
    M4x4        LocalTransform;
    s32         Parent;
    M4x4        InverseBindPose;
};
struct skeleton
{
    joint*  Joints;
    u32     JointsCount;
};

struct joint_pose
{
    quaternion Rotation;
    Vec3       Translation;
    Vec3       Scaling;
};
struct skeleton_pose
{
    skeleton*   Skeleton;
    joint_pose* LocalPoses;
    M4x4*       ModelSpaceMatrices;
    M4x4*       SkinningMatrices;
};

typedef struct joint_pose sample;
struct animation_clip
{
    u32     JointsCount;
    u32     SamplesCount;
    sample* Samples; // [JointsCount][SamplesCount]
};
