/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#ifndef ECG_AES_H
#define ECG_AES_H

#include "ecg_Core.h"
#include "ecg_NonCopyable.h"

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
/**
 * AES密码大小，以32位字为单位
 */
enum AesKeySize {
    aks4Words = 4, /**< 对应128位 */
    aks6Words = 6, /**< 对应192位 */
    aks8Words = 8, /**< 对应256位 */
};

////////////////////////////////////////////////////////////////////////////////
/**
 * 高级加密标准 Advanced Encryption Standard (AES)
 *
 * 翻译自James McCaffrey的C#版本，并用C++模板优化
 * 更多内容：http://msdn.microsoft.com/zh-cn/magazine/cc164055(en-us).aspx
 */
template <AesKeySize Nk, byte Nr>
class Aes : public NonCopyable
{
public:
    enum {
        /**< 密码字节数，分为三种：16、24以及32字节，
             分别对应128，192以及256位 */
        KeyByteCount = Nk * 4, 
        /**< 分组字节数，固定16字节，即128位 */
        BlockByteCount = 16, 
    };

#pragma pack(push, 1)
    typedef byte Key[KeyByteCount]; /**< 密码数组 */
    typedef byte Block[BlockByteCount]; /**< 分组数组 */
#pragma pack(pop)
public:
    /**
     * 构造器
     */
    Aes()
    {
        initialize();
    }

    /**
     * 析构器
     */
    ~Aes()
    {
    }

    /**
     * 设置种子密码
     * @param[in] key 种子密码
     */
    void setKey(const Key& key)
    {
        keyExpansion(key); // 扩展种子密码到密码调度表中
    }

    /**
     * 加密一个分组
     * @param[in] plainBlock 待加密的分组
     * @param[out] cipherBlock 已经加密的分组
     */
    void encipher(const Block& plainBlock, Block& cipherBlock)
    {
        // 使用input来初始化State矩阵
        m_stateMatrix[0 % 4][0 / 4] = plainBlock[0];
        m_stateMatrix[1 % 4][1 / 4] = plainBlock[1];
        m_stateMatrix[2 % 4][2 / 4] = plainBlock[2];
        m_stateMatrix[3 % 4][3 / 4] = plainBlock[3];
        m_stateMatrix[4 % 4][4 / 4] = plainBlock[4];
        m_stateMatrix[5 % 4][5 / 4] = plainBlock[5];
        m_stateMatrix[6 % 4][6 / 4] = plainBlock[6];
        m_stateMatrix[7 % 4][7 / 4] = plainBlock[7];
        m_stateMatrix[8 % 4][8 / 4] = plainBlock[8];
        m_stateMatrix[9 % 4][9 / 4] = plainBlock[9];
        m_stateMatrix[10 % 4][10 / 4] = plainBlock[10];
        m_stateMatrix[11 % 4][11 / 4] = plainBlock[11];
        m_stateMatrix[12 % 4][12 / 4] = plainBlock[12];
        m_stateMatrix[13 % 4][13 / 4] = plainBlock[13];
        m_stateMatrix[14 % 4][14 / 4] = plainBlock[14];
        m_stateMatrix[15 % 4][15 / 4] = plainBlock[15];

        addRoundKey(0);

        // 轮循环
        for (size_t round = 1; round <= Nr - 1; ++round) {
            subBytes(); 
            shiftRows();  
            mixColumns(); 
            addRoundKey(round);
        }

        subBytes();
        shiftRows();
        addRoundKey(Nr);

        // 将State矩阵转化为output
        cipherBlock[0] = m_stateMatrix[0 % 4][0 / 4];
        cipherBlock[1] = m_stateMatrix[1 % 4][1 / 4];
        cipherBlock[2] = m_stateMatrix[2 % 4][2 / 4];
        cipherBlock[3] = m_stateMatrix[3 % 4][3 / 4];
        cipherBlock[4] = m_stateMatrix[4 % 4][4 / 4];
        cipherBlock[5] = m_stateMatrix[5 % 4][5 / 4];
        cipherBlock[6] = m_stateMatrix[6 % 4][6 / 4];
        cipherBlock[7] = m_stateMatrix[7 % 4][7 / 4];
        cipherBlock[8] = m_stateMatrix[8 % 4][8 / 4];
        cipherBlock[9] = m_stateMatrix[9 % 4][9 / 4];
        cipherBlock[10] = m_stateMatrix[10 % 4][10 / 4];
        cipherBlock[11] = m_stateMatrix[11 % 4][11 / 4];
        cipherBlock[12] = m_stateMatrix[12 % 4][12 / 4];
        cipherBlock[13] = m_stateMatrix[13 % 4][13 / 4];
        cipherBlock[14] = m_stateMatrix[14 % 4][14 / 4];
        cipherBlock[15] = m_stateMatrix[15 % 4][15 / 4];
    }

    /**
     * 加密多个分组
     * @param[in] plainBlock 待加密的多个分组
     * @param[out] cipherBlock 已经加密的多个分组
     * @param[in] count 分组数量
     */
    void encipher(const Block plainBlocks[], Block cipherBlocks[], size_t count)
    {
        for (size_t i = 0; i < count; ++i) {
            encipher(plainBlocks[i], cipherBlocks[i]);
        }
    }


    /**
     * 加密缓冲区
     * @param[in] plainBuffer 待加密的缓冲区
     * @param[out] cipherBuffer 已经加密的缓冲区
     * @param[in] count 缓冲区大小
     * @return 成功返回true，否则返回false
     */
    bool encipher(const void* plainBuffer, void* cipherBuffer, size_t size)
    {
        if (size > 0 && (size % BlockByteCount) == 0) {
            encipher(reinterpret_cast<const Block*>(plainBuffer), 
                reinterpret_cast<Block*>(cipherBuffer), size / BlockByteCount);
            return true;
        }
        else {
            assert_param(false);
        }
        return false;
    }


    /**
     * 解密一个分组
     * @param[in] input 待解密的分组
     * @param[out] output 已经解密的分组
     */
    void decipher(const Block& cipherBlock, Block& plainBlock)
    {
        // 使用input来初始化State矩阵
        m_stateMatrix[0 % 4][0 / 4] = cipherBlock[0];
        m_stateMatrix[1 % 4][1 / 4] = cipherBlock[1];
        m_stateMatrix[2 % 4][2 / 4] = cipherBlock[2];
        m_stateMatrix[3 % 4][3 / 4] = cipherBlock[3];
        m_stateMatrix[4 % 4][4 / 4] = cipherBlock[4];
        m_stateMatrix[5 % 4][5 / 4] = cipherBlock[5];
        m_stateMatrix[6 % 4][6 / 4] = cipherBlock[6];
        m_stateMatrix[7 % 4][7 / 4] = cipherBlock[7];
        m_stateMatrix[8 % 4][8 / 4] = cipherBlock[8];
        m_stateMatrix[9 % 4][9 / 4] = cipherBlock[9];
        m_stateMatrix[10 % 4][10 / 4] = cipherBlock[10];
        m_stateMatrix[11 % 4][11 / 4] = cipherBlock[11];
        m_stateMatrix[12 % 4][12 / 4] = cipherBlock[12];
        m_stateMatrix[13 % 4][13 / 4] = cipherBlock[13];
        m_stateMatrix[14 % 4][14 / 4] = cipherBlock[14];
        m_stateMatrix[15 % 4][15 / 4] = cipherBlock[15];

        addRoundKey(Nr);

        // 轮循环
        for (size_t round = Nr - 1; round >= 1; --round) {
            inverseShiftRows();
            inverseSubBytes();
            addRoundKey(round);
            inverseMixColumns();
        }

        inverseShiftRows();
        inverseSubBytes();
        addRoundKey(0);

        // 将State矩阵转化为output
        plainBlock[0] = m_stateMatrix[0 % 4][0 / 4];
        plainBlock[1] = m_stateMatrix[1 % 4][1 / 4];
        plainBlock[2] = m_stateMatrix[2 % 4][2 / 4];
        plainBlock[3] = m_stateMatrix[3 % 4][3 / 4];
        plainBlock[4] = m_stateMatrix[4 % 4][4 / 4];
        plainBlock[5] = m_stateMatrix[5 % 4][5 / 4];
        plainBlock[6] = m_stateMatrix[6 % 4][6 / 4];
        plainBlock[7] = m_stateMatrix[7 % 4][7 / 4];
        plainBlock[8] = m_stateMatrix[8 % 4][8 / 4];
        plainBlock[9] = m_stateMatrix[9 % 4][9 / 4];
        plainBlock[10] = m_stateMatrix[10 % 4][10 / 4];
        plainBlock[11] = m_stateMatrix[11 % 4][11 / 4];
        plainBlock[12] = m_stateMatrix[12 % 4][12 / 4];
        plainBlock[13] = m_stateMatrix[13 % 4][13 / 4];
        plainBlock[14] = m_stateMatrix[14 % 4][14 / 4];
        plainBlock[15] = m_stateMatrix[15 % 4][15 / 4];
    }

    /**
     * 解密多个分组
     * @param[in] input 待解密的多个分组
     * @param[out] output 已经解密的多个分组
     * @param[in] count 分组数量
     */
    void decipher(const Block cipherBlocks[], Block plainBlocks[], size_t count)
    {
        for (size_t i = 0; i < count; ++i) {
            decipher(cipherBlocks[i], plainBlocks[i]);
        }
    }

    /**
     * 解密缓冲区
     * @param[in] input 待解密的缓冲区
     * @param[out] output 已经解密的缓冲区
     * @param[in] count 缓冲区大小
     * @return 成功返回true，否则返回false
     */
    bool decipher(const void* cipherBuffer, void* plainBuffer, size_t size)
    {
        if (size > 0 && (size % BlockByteCount) == 0) {
            decipher(reinterpret_cast<const Block*>(cipherBuffer), 
                reinterpret_cast<Block*>(plainBuffer), size / BlockByteCount);
            return true;
        }
        else {
            assert_param(false);
        }
        return false;
    }
private:
    enum {
        Nb = 4, // 分组大小，以32位字为单位，在AES中，这个值总是等于4，限128位
    };
private:
    void initialize()
    {
        if (!m_initialize) {
            m_initialize = true;
            initGfMultTable();
        }
    }

    void addRoundKey(size_t round)
    {
        m_stateMatrixInWords[0] ^= reinterpret_cast<dword&>(m_keySchedule[0][round * 4]);
        m_stateMatrixInWords[1] ^= reinterpret_cast<dword&>(m_keySchedule[1][round * 4]);
        m_stateMatrixInWords[2] ^= reinterpret_cast<dword&>(m_keySchedule[2][round * 4]);
        m_stateMatrixInWords[3] ^= reinterpret_cast<dword&>(m_keySchedule[3][round * 4]);
    }

    void subBytes()
    {
        // 第0行
        m_stateMatrix[0][0] = m_subBox[m_stateMatrix[0][0]];
        m_stateMatrix[0][1] = m_subBox[m_stateMatrix[0][1]];
        m_stateMatrix[0][2] = m_subBox[m_stateMatrix[0][2]];
        m_stateMatrix[0][3] = m_subBox[m_stateMatrix[0][3]];

        // 第1行
        m_stateMatrix[1][0] = m_subBox[m_stateMatrix[1][0]];
        m_stateMatrix[1][1] = m_subBox[m_stateMatrix[1][1]];
        m_stateMatrix[1][2] = m_subBox[m_stateMatrix[1][2]];
        m_stateMatrix[1][3] = m_subBox[m_stateMatrix[1][3]];

        // 第2行
        m_stateMatrix[2][0] = m_subBox[m_stateMatrix[2][0]];
        m_stateMatrix[2][1] = m_subBox[m_stateMatrix[2][1]];
        m_stateMatrix[2][2] = m_subBox[m_stateMatrix[2][2]];
        m_stateMatrix[2][3] = m_subBox[m_stateMatrix[2][3]];

        // 第3行
        m_stateMatrix[3][0] = m_subBox[m_stateMatrix[3][0]];
        m_stateMatrix[3][1] = m_subBox[m_stateMatrix[3][1]];
        m_stateMatrix[3][2] = m_subBox[m_stateMatrix[3][2]];
        m_stateMatrix[3][3] = m_subBox[m_stateMatrix[3][3]];
    }

    void inverseSubBytes()
    {
        // 第0行
        m_stateMatrix[0][0] = m_inverseSubBox[m_stateMatrix[0][0]];
        m_stateMatrix[0][1] = m_inverseSubBox[m_stateMatrix[0][1]];
        m_stateMatrix[0][2] = m_inverseSubBox[m_stateMatrix[0][2]];
        m_stateMatrix[0][3] = m_inverseSubBox[m_stateMatrix[0][3]];

        // 第1行
        m_stateMatrix[1][0] = m_inverseSubBox[m_stateMatrix[1][0]];
        m_stateMatrix[1][1] = m_inverseSubBox[m_stateMatrix[1][1]];
        m_stateMatrix[1][2] = m_inverseSubBox[m_stateMatrix[1][2]];
        m_stateMatrix[1][3] = m_inverseSubBox[m_stateMatrix[1][3]];

        // 第2行
        m_stateMatrix[2][0] = m_inverseSubBox[m_stateMatrix[2][0]];
        m_stateMatrix[2][1] = m_inverseSubBox[m_stateMatrix[2][1]];
        m_stateMatrix[2][2] = m_inverseSubBox[m_stateMatrix[2][2]];
        m_stateMatrix[2][3] = m_inverseSubBox[m_stateMatrix[2][3]];

        // 第3行
        m_stateMatrix[3][0] = m_inverseSubBox[m_stateMatrix[3][0]];
        m_stateMatrix[3][1] = m_inverseSubBox[m_stateMatrix[3][1]];
        m_stateMatrix[3][2] = m_inverseSubBox[m_stateMatrix[3][2]];
        m_stateMatrix[3][3] = m_inverseSubBox[m_stateMatrix[3][3]];
    }

    void shiftRows()
    {
        // 小端模式下，相当于4字节数组左移
        rotateRight(m_stateMatrixInWords[1], 8);
        rotateRight(m_stateMatrixInWords[2], 16);
        rotateRight(m_stateMatrixInWords[3], 24);
    }

    void inverseShiftRows()
    {
        // 小端模式下，相当于4字节数组右移
        rotateLeft(m_stateMatrixInWords[1], 8);
        rotateLeft(m_stateMatrixInWords[2], 16);
        rotateLeft(m_stateMatrixInWords[3], 24);
    }

    void mixColumns()
    {
        m_tempStateMatirixInWords[0] = m_stateMatrixInWords[0];
        m_tempStateMatirixInWords[1] = m_stateMatrixInWords[1];
        m_tempStateMatirixInWords[2] = m_stateMatrixInWords[2];
        m_tempStateMatirixInWords[3] = m_stateMatrixInWords[3];

        // 第0列
        m_stateMatrix[0][0] = m_gfMultBy02[m_tempStateMatrix[0][0]] 
                                ^ m_gfMultBy03[m_tempStateMatrix[1][0]] 
                                ^ m_tempStateMatrix[2][0] 
                                ^ m_tempStateMatrix[3][0];
        m_stateMatrix[1][0] = m_tempStateMatrix[0][0] 
                                ^ m_gfMultBy02[m_tempStateMatrix[1][0]] 
                                ^ m_gfMultBy03[m_tempStateMatrix[2][0]] 
                                ^ m_tempStateMatrix[3][0];
        m_stateMatrix[2][0] = m_tempStateMatrix[0][0] ^ m_tempStateMatrix[1][0] 
                                ^ m_gfMultBy02[m_tempStateMatrix[2][0]] 
                                ^ m_gfMultBy03[m_tempStateMatrix[3][0]];
        m_stateMatrix[3][0] = m_gfMultBy03[m_tempStateMatrix[0][0]] 
                                ^ m_tempStateMatrix[1][0] ^ m_tempStateMatrix[2][0] 
                                ^ m_gfMultBy02[m_tempStateMatrix[3][0]];

        // 第1列
        m_stateMatrix[0][1] = m_gfMultBy02[m_tempStateMatrix[0][1]] 
                                ^ m_gfMultBy03[m_tempStateMatrix[1][1]] 
                                ^ m_tempStateMatrix[2][1] ^ m_tempStateMatrix[3][1];
        m_stateMatrix[1][1] = m_tempStateMatrix[0][1] 
                                ^ m_gfMultBy02[m_tempStateMatrix[1][1]] 
                                ^ m_gfMultBy03[m_tempStateMatrix[2][1]] 
                                ^ m_tempStateMatrix[3][1];
        m_stateMatrix[2][1] = m_tempStateMatrix[0][1] ^ m_tempStateMatrix[1][1] 
                                ^ m_gfMultBy02[m_tempStateMatrix[2][1]] 
                                ^ m_gfMultBy03[m_tempStateMatrix[3][1]];
        m_stateMatrix[3][1] = m_gfMultBy03[m_tempStateMatrix[0][1]] 
                                ^ m_tempStateMatrix[1][1] ^ m_tempStateMatrix[2][1] 
                                ^ m_gfMultBy02[m_tempStateMatrix[3][1]];

        // 第2列
        m_stateMatrix[0][2] = m_gfMultBy02[m_tempStateMatrix[0][2]] 
                                ^ m_gfMultBy03[m_tempStateMatrix[1][2]] 
                                ^ m_tempStateMatrix[2][2] ^ m_tempStateMatrix[3][2];
        m_stateMatrix[1][2] = m_tempStateMatrix[0][2] 
                                ^ m_gfMultBy02[m_tempStateMatrix[1][2]] 
                                ^ m_gfMultBy03[m_tempStateMatrix[2][2]] 
                                ^ m_tempStateMatrix[3][2];
        m_stateMatrix[2][2] = m_tempStateMatrix[0][2] ^ m_tempStateMatrix[1][2] 
                                ^ m_gfMultBy02[m_tempStateMatrix[2][2]] 
                                ^ m_gfMultBy03[m_tempStateMatrix[3][2]];
        m_stateMatrix[3][2] = m_gfMultBy03[m_tempStateMatrix[0][2]] 
                                ^ m_tempStateMatrix[1][2] ^ m_tempStateMatrix[2][2] 
                                ^ m_gfMultBy02[m_tempStateMatrix[3][2]];

        // 第3列
        m_stateMatrix[0][3] = m_gfMultBy02[m_tempStateMatrix[0][3]] 
                                ^ m_gfMultBy03[m_tempStateMatrix[1][3]] 
                                ^ m_tempStateMatrix[2][3] ^ m_tempStateMatrix[3][3];
        m_stateMatrix[1][3] = m_tempStateMatrix[0][3] 
                                ^ m_gfMultBy02[m_tempStateMatrix[1][3]] 
                                ^ m_gfMultBy03[m_tempStateMatrix[2][3]] 
                                ^ m_tempStateMatrix[3][3];
        m_stateMatrix[2][3] = m_tempStateMatrix[0][3] ^ m_tempStateMatrix[1][3] 
                                ^ m_gfMultBy02[m_tempStateMatrix[2][3]] 
                                ^ m_gfMultBy03[m_tempStateMatrix[3][3]];
        m_stateMatrix[3][3] = m_gfMultBy03[m_tempStateMatrix[0][3]] 
                                ^ m_tempStateMatrix[1][3] ^ m_tempStateMatrix[2][3] 
                                ^ m_gfMultBy02[m_tempStateMatrix[3][3]];
    }

    void inverseMixColumns()
    {
        m_tempStateMatirixInWords[0] = m_stateMatrixInWords[0];
        m_tempStateMatirixInWords[1] = m_stateMatrixInWords[1];
        m_tempStateMatirixInWords[2] = m_stateMatrixInWords[2];
        m_tempStateMatirixInWords[3] = m_stateMatrixInWords[3];

        // 第0列
        m_stateMatrix[0][0] = m_gfMultBy0e[m_tempStateMatrix[0][0]] 
                                ^ m_gfMultBy0b[m_tempStateMatrix[1][0]] 
                                ^ m_gfMultBy0d[m_tempStateMatrix[2][0]] 
                                ^ m_gfMultBy09[m_tempStateMatrix[3][0]];
        m_stateMatrix[1][0] = m_gfMultBy09[m_tempStateMatrix[0][0]] 
                                ^ m_gfMultBy0e[m_tempStateMatrix[1][0]] 
                                ^ m_gfMultBy0b[m_tempStateMatrix[2][0]] 
                                ^ m_gfMultBy0d[m_tempStateMatrix[3][0]];
        m_stateMatrix[2][0] = m_gfMultBy0d[m_tempStateMatrix[0][0]] 
                                ^ m_gfMultBy09[m_tempStateMatrix[1][0]] 
                                ^ m_gfMultBy0e[m_tempStateMatrix[2][0]] 
                                ^ m_gfMultBy0b[m_tempStateMatrix[3][0]];
        m_stateMatrix[3][0] = m_gfMultBy0b[m_tempStateMatrix[0][0]] 
                                ^ m_gfMultBy0d[m_tempStateMatrix[1][0]] 
                                ^ m_gfMultBy09[m_tempStateMatrix[2][0]] 
                                ^ m_gfMultBy0e[m_tempStateMatrix[3][0]];

        // 第0列
        m_stateMatrix[0][1] = m_gfMultBy0e[m_tempStateMatrix[0][1]] 
                                ^ m_gfMultBy0b[m_tempStateMatrix[1][1]] 
                                ^ m_gfMultBy0d[m_tempStateMatrix[2][1]] 
                                ^ m_gfMultBy09[m_tempStateMatrix[3][1]];
        m_stateMatrix[1][1] = m_gfMultBy09[m_tempStateMatrix[0][1]] 
                                ^ m_gfMultBy0e[m_tempStateMatrix[1][1]] 
                                ^ m_gfMultBy0b[m_tempStateMatrix[2][1]] 
                                ^ m_gfMultBy0d[m_tempStateMatrix[3][1]];
        m_stateMatrix[2][1] = m_gfMultBy0d[m_tempStateMatrix[0][1]] 
                                ^ m_gfMultBy09[m_tempStateMatrix[1][1]] 
                                ^ m_gfMultBy0e[m_tempStateMatrix[2][1]] 
                                ^ m_gfMultBy0b[m_tempStateMatrix[3][1]];
        m_stateMatrix[3][1] = m_gfMultBy0b[m_tempStateMatrix[0][1]] 
                                ^ m_gfMultBy0d[m_tempStateMatrix[1][1]] 
                                ^ m_gfMultBy09[m_tempStateMatrix[2][1]] 
                                ^ m_gfMultBy0e[m_tempStateMatrix[3][1]];

        // 第0列
        m_stateMatrix[0][2] = m_gfMultBy0e[m_tempStateMatrix[0][2]] 
                                ^ m_gfMultBy0b[m_tempStateMatrix[1][2]] 
                                ^ m_gfMultBy0d[m_tempStateMatrix[2][2]] 
                                ^ m_gfMultBy09[m_tempStateMatrix[3][2]];
        m_stateMatrix[1][2] = m_gfMultBy09[m_tempStateMatrix[0][2]] 
                                ^ m_gfMultBy0e[m_tempStateMatrix[1][2]] 
                                ^ m_gfMultBy0b[m_tempStateMatrix[2][2]] 
                                ^ m_gfMultBy0d[m_tempStateMatrix[3][2]];
        m_stateMatrix[2][2] = m_gfMultBy0d[m_tempStateMatrix[0][2]] 
                                ^ m_gfMultBy09[m_tempStateMatrix[1][2]] 
                                ^ m_gfMultBy0e[m_tempStateMatrix[2][2]] 
                                ^ m_gfMultBy0b[m_tempStateMatrix[3][2]];
        m_stateMatrix[3][2] = m_gfMultBy0b[m_tempStateMatrix[0][2]] 
                                ^ m_gfMultBy0d[m_tempStateMatrix[1][2]] 
                                ^ m_gfMultBy09[m_tempStateMatrix[2][2]] 
                                ^ m_gfMultBy0e[m_tempStateMatrix[3][2]];

        // 第0列
        m_stateMatrix[0][3] = m_gfMultBy0e[m_tempStateMatrix[0][3]] 
                                ^ m_gfMultBy0b[m_tempStateMatrix[1][3]] 
                                ^ m_gfMultBy0d[m_tempStateMatrix[2][3]] 
                                ^ m_gfMultBy09[m_tempStateMatrix[3][3]];
        m_stateMatrix[1][3] = m_gfMultBy09[m_tempStateMatrix[0][3]] 
                                ^ m_gfMultBy0e[m_tempStateMatrix[1][3]] 
                                ^ m_gfMultBy0b[m_tempStateMatrix[2][3]] 
                                ^ m_gfMultBy0d[m_tempStateMatrix[3][3]];
        m_stateMatrix[2][3] = m_gfMultBy0d[m_tempStateMatrix[0][3]] 
                                ^ m_gfMultBy09[m_tempStateMatrix[1][3]] 
                                ^ m_gfMultBy0e[m_tempStateMatrix[2][3]] 
                                ^ m_gfMultBy0b[m_tempStateMatrix[3][3]];
        m_stateMatrix[3][3] = m_gfMultBy0b[m_tempStateMatrix[0][3]] 
                                ^ m_gfMultBy0d[m_tempStateMatrix[1][3]] 
                                ^ m_gfMultBy09[m_tempStateMatrix[2][3]] 
                                ^ m_gfMultBy0e[m_tempStateMatrix[3][3]];
    }

    void rotateLeft(dword& data, byte n)
    {
        data = (data >> (32 - n)) | (data << n);
    }

    void rotateRight(dword& data, byte n)
    {
        data = (data << (32 - n)) | (data >> n);
    }

    byte gfMultBy02(byte data)
    {
        return data < 0x80 ? data << 1 : (data << 1) ^ 0x1b;
    }

    byte gfMultBy03(byte data)
    {
        return gfMultBy02(data) ^ data;
    }

    byte gfMultBy09(byte data)
    {
        return gfMultBy02(gfMultBy02(gfMultBy02(data))) ^ data ;
    }

    byte gfMultBy0b(byte data)
    {
        return gfMultBy02(gfMultBy02(gfMultBy02(data))) ^ 
            gfMultBy02(data) ^ data;
    }

    byte gfMultBy0d(byte data)
    {
        return gfMultBy02(gfMultBy02(gfMultBy02(data))) ^ 
            gfMultBy02(gfMultBy02(data)) ^ data;
    }

    byte gfMultBy0e(byte data)
    {
        return gfMultBy02(gfMultBy02(gfMultBy02(data))) ^ 
            gfMultBy02(gfMultBy02(data)) ^ gfMultBy02(data);
    }

    void initGfMultTable()
    {
        for (size_t i = 0; i < 256; ++i) {
            m_gfMultBy02[i] = gfMultBy02(i);
            m_gfMultBy03[i] = gfMultBy03(i);
            m_gfMultBy09[i] = gfMultBy09(i);
            m_gfMultBy0b[i] = gfMultBy0b(i);
            m_gfMultBy0d[i] = gfMultBy0d(i);
            m_gfMultBy0e[i] = gfMultBy0e(i);
        }
    }

    void keyExpansion(const Key& key)
    {
        for (size_t col = 0; col < Nk; ++col) {
            m_keySchedule[0][col] = key[4 * col];
            m_keySchedule[1][col] = key[4 * col + 1];
            m_keySchedule[2][col] = key[4 * col + 2];
            m_keySchedule[3][col] = key[4 * col + 3];
        }

        byte temp[4];

        for (size_t col = Nk; col < Nb * (Nr + 1); ++col) {
            temp[0] = m_keySchedule[0][col - 1];
            temp[1] = m_keySchedule[1][col - 1];
            temp[2] = m_keySchedule[2][col - 1];
            temp[3] = m_keySchedule[3][col - 1];

            if (col % Nk == 0) {
                rotWord(temp);
                subWord(temp);
                temp[0] ^= m_roundConst[col / Nk][0];
                temp[1] ^= m_roundConst[col / Nk][1];
                temp[2] ^= m_roundConst[col / Nk][2];
                temp[3] ^= m_roundConst[col / Nk][3];
            }
            else if ( Nk > 6 && (col % Nk == 4) ) {
                subWord(temp);
            }

            m_keySchedule[0][col] = m_keySchedule[0][col - Nk] ^ temp[0];
            m_keySchedule[1][col] = m_keySchedule[1][col - Nk] ^ temp[1];
            m_keySchedule[2][col] = m_keySchedule[2][col - Nk] ^ temp[2];
            m_keySchedule[3][col] = m_keySchedule[3][col - Nk] ^ temp[3];
        }
    }

    void subWord(byte* word)
    {
        word[0] = m_subBox[word[0]];
        word[1] = m_subBox[word[1]];
        word[2] = m_subBox[word[2]];
        word[3] = m_subBox[word[3]];
    }

    void rotWord(byte* word)
    {
        rotateRight(reinterpret_cast<dword&>(*word), 8);
    }
private:
    static bool m_initialize;
    static byte m_subBox[256];   // S盒
    static byte m_inverseSubBox[256];  // 逆S盒
    static byte m_roundConst[11][4]; // 轮常数
    static byte m_gfMultBy02[256];
    static byte m_gfMultBy03[256];
    static byte m_gfMultBy09[256];
    static byte m_gfMultBy0b[256];
    static byte m_gfMultBy0d[256];
    static byte m_gfMultBy0e[256];
    union {
        byte m_stateMatrix[4][Nb];  // 状态矩阵
        dword m_stateMatrixInWords[4];
    };
    union {
        byte m_tempStateMatrix[4][Nb]; // 临时的状态矩阵，用于优化
        dword m_tempStateMatirixInWords[4];
    };
    byte m_keySchedule[4][Nb * (Nr + 1)]; // 密码调度表
};

////////////////////////////////////////////////////////////////////////////////
template <AesKeySize Nk, byte Nr> bool Aes<Nk, Nr>::m_initialize = false;

template <AesKeySize Nk, byte Nr> byte Aes<Nk, Nr>::m_subBox[256] = {
    /*      0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f */
    /*0*/  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    /*1*/  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    /*2*/  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    /*3*/  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    /*4*/  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    /*5*/  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    /*6*/  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    /*7*/  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    /*8*/  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    /*9*/  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    /*a*/  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    /*b*/  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    /*c*/  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    /*d*/  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    /*e*/  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    /*f*/  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

template <AesKeySize Nk, byte Nr> byte Aes<Nk, Nr>::m_inverseSubBox[256] = {
    /*      0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f */
    /*0*/  0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    /*1*/  0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    /*2*/  0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    /*3*/  0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    /*4*/  0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    /*5*/  0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    /*6*/  0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    /*7*/  0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    /*8*/  0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    /*9*/  0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    /*a*/  0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    /*b*/  0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    /*c*/  0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    /*d*/  0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    /*e*/  0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    /*f*/  0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

template <AesKeySize Nk, byte Nr> byte Aes<Nk, Nr>::m_roundConst[11][4] = {
    {0x00, 0x00, 0x00, 0x00},  
    {0x01, 0x00, 0x00, 0x00},
    {0x02, 0x00, 0x00, 0x00},
    {0x04, 0x00, 0x00, 0x00},
    {0x08, 0x00, 0x00, 0x00},
    {0x10, 0x00, 0x00, 0x00},
    {0x20, 0x00, 0x00, 0x00},
    {0x40, 0x00, 0x00, 0x00},
    {0x80, 0x00, 0x00, 0x00},
    {0x1b, 0x00, 0x00, 0x00},
    {0x36, 0x00, 0x00, 0x00}
};

template <AesKeySize Nk, byte Nr> byte Aes<Nk, Nr>::m_gfMultBy02[256];
template <AesKeySize Nk, byte Nr> byte Aes<Nk, Nr>::m_gfMultBy03[256];
template <AesKeySize Nk, byte Nr> byte Aes<Nk, Nr>::m_gfMultBy09[256];
template <AesKeySize Nk, byte Nr> byte Aes<Nk, Nr>::m_gfMultBy0b[256];
template <AesKeySize Nk, byte Nr> byte Aes<Nk, Nr>::m_gfMultBy0d[256];
template <AesKeySize Nk, byte Nr> byte Aes<Nk, Nr>::m_gfMultBy0e[256];

////////////////////////////////////////////////////////////////////////////////
typedef Aes<aks4Words, 10> Aes128;
typedef Aes<aks6Words, 12> Aes192;
typedef Aes<aks8Words, 14> Aes256;

} // namespace

#endif // ECG_AES_H