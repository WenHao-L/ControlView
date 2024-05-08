#include <windows.h>
#include <wmsdkidl.h>
#include <assert.h>
#include <atlbase.h>
#include "nncam.h"


class CWmvRecord
{
    const LONG m_lFrameWidth;
    const LONG m_lFrameHeight;
	// Windows Media Format SDK中用于写入媒体文件的接口
	CComPtr<IWMWriter>		m_spIWMWriter;
public:
	CWmvRecord(LONG lFrameWidth, LONG lFrameHeight)
	: m_lFrameWidth(lFrameWidth), m_lFrameHeight(lFrameHeight)
	{
	}

	BOOL StartRecord(const wchar_t* strFilename, DWORD dwBitrate)
	{
		// 声明一个智能指针spIWMProfileManager，用于管理配置文件
		CComPtr<IWMProfileManager> spIWMProfileManager;
		// 调用WMCreateProfileManager函数创建一个配置文件管理器，并将结果存储在hr中
		HRESULT hr = WMCreateProfileManager(&spIWMProfileManager);
		if (FAILED(hr))
			return FALSE;

		// 声明一个智能指针spIWMCodecInfo，用于获取编解码器信息
		CComPtr<IWMCodecInfo> spIWMCodecInfo;
		// 通过QueryInterface获取IWMProfileManager的IWMCodecInfo接口，并检查结果
		hr = spIWMProfileManager->QueryInterface(__uuidof(IWMCodecInfo), (void**)&spIWMCodecInfo);
		if (FAILED(hr))
			return FALSE;
		
		// 声明一个变量cCodecs，用于存储编解码器的数量
		DWORD cCodecs = 0;
		// 获取视频编解码器的数量，并将结果存储在hr中
        hr = spIWMCodecInfo->GetCodecInfoCount(WMMEDIATYPE_Video, &cCodecs);
		if (FAILED(hr))
			return FALSE;

		// 声明一个智能指针spIWMStreamConfig，用于配置流
		CComPtr<IWMStreamConfig> spIWMStreamConfig;
        //
        // Search from the last codec because the last codec usually
        // is the newest codec.
        // 循环遍历所有编解码器，从最后一个开始向前遍历
        for (int i = cCodecs - 1; i >= 0; i--)
        {
			// 获取指定编解码器的格式数量
            DWORD cFormats;
            hr = spIWMCodecInfo->GetCodecFormatCount(WMMEDIATYPE_Video, i, &cFormats);
			if (FAILED(hr))
				break;
			
			// 循环遍历所有格式
            for (DWORD j = 0; j < cFormats; j++)
			{
				// 获取指定编解码器和格式的配置
                hr = spIWMCodecInfo->GetCodecFormat(WMMEDIATYPE_Video, i, j, &spIWMStreamConfig);
				if (FAILED(hr))
					break;

				// 使用指定的比特率配置输入流
				hr = ConfigureInput(spIWMStreamConfig, dwBitrate);
				if (SUCCEEDED(hr))
					break;
				// 如果配置失败，则重置智能指针
				spIWMStreamConfig = NULL;
			}
			// 如果智能指针不是NULL，则表示找到了合适的配置，跳出循环
			if (spIWMStreamConfig)
				break;
		}
		// 如果最终没有找到合适的配置，则返回FALSE
		if (spIWMStreamConfig == NULL)
			return FALSE;
		
		// 声明一个智能指针spIWMProfile，用于管理配置文件
		CComPtr<IWMProfile> spIWMProfile;
		// 创建一个空的配置文件
		hr = spIWMProfileManager->CreateEmptyProfile(WMT_VER_8_0, &spIWMProfile);
		if (FAILED(hr))
			return FALSE;

		{
			// 创建一个新的视频流
			CComPtr<IWMStreamConfig> spIWMStreamConfig2;
			hr = spIWMProfile->CreateNewStream(WMMEDIATYPE_Video, &spIWMStreamConfig2);
			if (FAILED(hr))
				return FALSE;

			// 获取流的编号
			WORD wStreamNum = 1;
			hr = spIWMStreamConfig2->GetStreamNumber(&wStreamNum);
			if (FAILED(hr))
				return FALSE;
			
			// 将编号设置到流配置中
			hr = spIWMStreamConfig->SetStreamNumber(wStreamNum);
			if (FAILED(hr))
				return FALSE;
		}
		// 设置流的比特率
		spIWMStreamConfig->SetBitrate(dwBitrate);

		// 将配置好的流添加到配置文件中
		hr = spIWMProfile->AddStream(spIWMStreamConfig);
		if (FAILED(hr))
			return FALSE;
		
		// 创建一个媒体写入器
		hr = WMCreateWriter(NULL, &m_spIWMWriter);
		if (FAILED(hr))
			return FALSE;
		
		// 为写入器设置配置文件
		hr = m_spIWMWriter->SetProfile(spIWMProfile);
		if (FAILED(hr))
			return FALSE;
		
		// 设置输入属性
		hr = SetInputProps();
		if (FAILED(hr))
			return FALSE;
		
		// 设置输出文件名
		hr = m_spIWMWriter->SetOutputFilename(strFilename);
		if (FAILED(hr))
			return FALSE;
		
		// 开始写入
		hr = m_spIWMWriter->BeginWriting();
		if (FAILED(hr))
			return FALSE;

		{
			// 尝试获取写入器的高级接口
			CComPtr<IWMWriterAdvanced> spIWMWriterAdvanced;
			m_spIWMWriter->QueryInterface(__uuidof(IWMWriterAdvanced), (void**)&spIWMWriterAdvanced);
			// 如果获取成功，设置写入器为实时源
			if (spIWMWriterAdvanced)
				spIWMWriterAdvanced->SetLiveSource(TRUE);
		}

		return TRUE;
	}

	void StopRecord()
	{
		// 检查m_spIWMWriter智能指针是否非空，即是否已经创建了写入器对象
		if (m_spIWMWriter)
		{
			// 调用写入器的Flush方法，确保所有待写入的数据都被写入到文件中
			m_spIWMWriter->Flush();
			// 调用写入器的EndWriting方法，结束写入过程
			m_spIWMWriter->EndWriting();
			// 将写入器智能指针设置为NULL，释放写入器对象
			m_spIWMWriter = NULL;
		}
	}

	BOOL WriteSample(const void* pData)
	{
		// 声明一个智能指针spINSSBuffer，用于管理INSSBuffer接口，该接口代表一个样本缓冲区
		CComPtr<INSSBuffer> spINSSBuffer;
		// 调用写入器的AllocateSample方法来分配一个样本缓冲区，检查操作是否成功
		if (SUCCEEDED(m_spIWMWriter->AllocateSample(TDIBWIDTHBYTES(m_lFrameWidth * 24) * m_lFrameHeight, &spINSSBuffer)))
		{
			// 声明一个BYTE指针pBuffer，用于后续操作
			BYTE* pBuffer = NULL;
			// 调用INSSBuffer的GetBuffer方法获取缓冲区的指针，检查操作是否成功
			if (SUCCEEDED(spINSSBuffer->GetBuffer(&pBuffer)))
			{
				// 使用memcpy函数将传入的视频样本数据pData复制到分配的样本缓冲区中
				memcpy(pBuffer, pData, TDIBWIDTHBYTES(m_lFrameWidth * 24) * m_lFrameHeight);
				// 设置样本缓冲区的长度，即样本数据的大小
				spINSSBuffer->SetLength(TDIBWIDTHBYTES(m_lFrameWidth * 24) * m_lFrameHeight);
				// 获取当前时间的毫秒数，作为样本的时间戳
				QWORD cnsSampleTime = GetTickCount();
				// 调用写入器的WriteSample方法将样本写入到媒体文件中，时间戳转换为100纳秒单位
				m_spIWMWriter->WriteSample(0, cnsSampleTime * 1000 * 10, 0, spINSSBuffer);
				return TRUE;
			}
		}

		return FALSE;
	}

private:
	HRESULT SetInputProps()
	{
		// 声明一个DWORD类型的变量dwFormats，用于存储输入格式的数量
		DWORD dwForamts = 0;
		// 调用写入器的GetInputFormatCount方法获取输入格式的数量，并将结果存储在hr中
		HRESULT hr = m_spIWMWriter->GetInputFormatCount(0, &dwForamts);
		if (FAILED(hr))
			return hr;
		
		// 循环遍历所有输入格式
		for (DWORD i = 0; i < dwForamts; ++i)
		{
			// 声明一个智能指针spIWMInputMediaProps，用于管理输入媒体属性
			CComPtr<IWMInputMediaProps> spIWMInputMediaProps;
			// 获取指定索引的输入格式，并存储结果
			hr = m_spIWMWriter->GetInputFormat(0, i, &spIWMInputMediaProps);
			if (FAILED(hr))
				return hr;
			
			// 声明一个DWORD类型的变量cbSize，用于存储媒体类型的大小
			DWORD cbSize = 0;
			// 获取媒体类型的所需大小，并将结果存储在cbSize中
			hr = spIWMInputMediaProps->GetMediaType(NULL, &cbSize);
			if (FAILED(hr))
				return hr;
			
			// 使用alloca函数动态分配内存以存储媒体类型信息
			WM_MEDIA_TYPE* pMediaType = (WM_MEDIA_TYPE*)alloca(cbSize);
			// 获取媒体类型信息，并将结果存储在hr中
			hr = spIWMInputMediaProps->GetMediaType(pMediaType, &cbSize);
			if (FAILED(hr))
				return hr;
			
			// 检查媒体子类型是否为WMMEDIASUBTYPE_RGB24，即是否为24位RGB格式
			if (pMediaType->subtype == WMMEDIASUBTYPE_RGB24)
			{
				// 设置媒体类型
				hr = spIWMInputMediaProps->SetMediaType(pMediaType);
				if (FAILED(hr))
					return hr;
				
				// 设置输入属性，并返回操作结果
				return m_spIWMWriter->SetInputProps(0, spIWMInputMediaProps);
			}
		}

		return E_FAIL;
	}

	HRESULT ConfigureInput(CComPtr<IWMStreamConfig>& spIWMStreamConfig, DWORD dwBitRate)
	{
		// 声明一个智能指针spIWMVideoMediaProps，用于获取视频媒体属性
		CComPtr<IWMVideoMediaProps> spIWMVideoMediaProps;
		// 通过QueryInterface从spIWMStreamConfig获取IWMVideoMediaProps接口
		HRESULT hr = spIWMStreamConfig->QueryInterface(__uuidof(IWMVideoMediaProps), (void**)&spIWMVideoMediaProps);
		if (FAILED(hr))
			return hr;
		
		// 声明一个变量cbMT，用于存储媒体类型的大小
		DWORD cbMT = 0;
		// 获取媒体类型的大小
		hr = spIWMVideoMediaProps->GetMediaType(NULL, &cbMT);
		if (FAILED(hr))
			return hr;

		// 使用alloca函数在栈上分配内存以存储媒体类型
		WM_MEDIA_TYPE* pType = (WM_MEDIA_TYPE*)alloca(cbMT);
		// 获取媒体类型信息
		hr = spIWMVideoMediaProps->GetMediaType(pType, &cbMT);
		// 检查获取媒体类型是否失败，格式类型是否为视频信息，或者格式数据指针是否为NULL。如果有误，返回E_FAIL。
		if (FAILED(hr) || (pType->formattype != WMFORMAT_VideoInfo) || (NULL == pType->pbFormat))
			return E_FAIL;
		
		// 声明一个布尔变量bFound，用于标记是否找到合适的压缩类型
		bool bFound = false;
		// First set pointers to the video structures.
		// 将媒体类型的格式数据转换为WMVIDEOINFOHEADER指针
		WMVIDEOINFOHEADER* pVidHdr = (WMVIDEOINFOHEADER*)pType->pbFormat;
		{
			// 定义一个静态数组FourCC，包含要查找的视频压缩格式的FourCC代码
			static const DWORD FourCC[] = {
				MAKEFOURCC('W', 'M', 'V', '3'),
				MAKEFOURCC('W', 'M', 'V', '2'),
				MAKEFOURCC('W', 'M', 'V', '1')
			};
			// 遍历FourCC数组，检查是否与当前视频头部的压缩类型匹配
			for (size_t i = 0; i < _countof(FourCC); ++i)
			{
				if (FourCC[i] == pVidHdr->bmiHeader.biCompression)
				{
					bFound = true;
					break;
				}
			}
		}
		// 如果没有找到匹配的压缩类型，则返回E_FAIL
		if (!bFound)
			return E_FAIL;

		// 设置视频的比特率
		pVidHdr->dwBitRate = dwBitRate;
		// 设置视频源和目标的宽度和高度
		pVidHdr->rcSource.right = m_lFrameWidth;
		pVidHdr->rcSource.bottom = m_lFrameHeight;
		pVidHdr->rcTarget.right = m_lFrameWidth;
		pVidHdr->rcTarget.bottom = m_lFrameHeight;

		// 获取指向位图信息头的指针
		BITMAPINFOHEADER* pbmi = &pVidHdr->bmiHeader;
		// 设置图像的宽度和高度
		pbmi->biWidth  = m_lFrameWidth;
		pbmi->biHeight = m_lFrameHeight;
    
		// Stride = (width * bytes/pixel), rounded to the next DWORD boundary.
		// 计算图像的跨度（Stride），即每行像素的字节数，并确保它是4字节对齐
		LONG lStride = (m_lFrameWidth * (pbmi->biBitCount / 8) + 3) & ~3;

		// Image size = stride * height.
		// 设置图像大小，即整个图像的总字节数
		pbmi->biSizeImage = m_lFrameHeight * lStride;

		// Apply the adjusted type to the video input.
		// 应用修改后的媒体类型
		hr = spIWMVideoMediaProps->SetMediaType(pType);
		if (FAILED(hr))
			return hr;

		/* you can change this quality */
		// 设置视频质量为100%
		spIWMVideoMediaProps->SetQuality(100);
		return hr;
	}
};
