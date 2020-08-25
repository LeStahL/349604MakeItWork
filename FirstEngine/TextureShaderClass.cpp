#include "textureshaderclass.h"

TextureShaderClass::TextureShaderClass() {

}

TextureShaderClass::~TextureShaderClass() {}

bool TextureShaderClass::Initialize(ID3D11Device* device, HWND hwnd) {
	bool result;

	// Initialize the vertex and pixel shaders.
	result = InitializeShader(device, hwnd, L"BasicTextureVertexShader.fx", L"BasicTexturePixelShader.fx");
	if (!result) {
		return false;
	}

	return true;
}

bool TextureShaderClass::Render(ID3D11DeviceContext* deviceContext, int indexCount, XMMATRIX worldMatrix, XMMATRIX viewMatrix, XMMATRIX projectionMatrix, ID3D11ShaderResourceView* texture) {
	bool result;

	// Set the shader parameters that it will use for rendering.
	result = SetShaderParameters(deviceContext, worldMatrix, viewMatrix, projectionMatrix, texture);
	if (!result) {
		return false;
	}

	// Now render the prepared buffers with the shader.
	RenderShader(deviceContext, indexCount);

	return true;
}

bool TextureShaderClass::InitializeShader(ID3D11Device* device, HWND hwnd, LPCWSTR vsFilename, LPCWSTR psFilename) {
	HRESULT result;

	D3D11_SAMPLER_DESC samplerDesc;

	// Initialize the pointers this function will use to null.
	Microsoft::WRL::ComPtr<ID3D10Blob> vertexErrorMessage;
	Microsoft::WRL::ComPtr<ID3D10Blob> pixelErrorMessage;
	Microsoft::WRL::ComPtr<ID3D10Blob> vertexShaderBuffer;
	Microsoft::WRL::ComPtr<ID3D10Blob> pixelShaderBuffer;

	// Compile the vertex shader code.
	result = D3DCompileFromFile(
		vsFilename,
		NULL,
		NULL,
		"TextureVertexShader",
		"vs_5_0",
		D3D10_SHADER_ENABLE_STRICTNESS,
		0,
		vertexShaderBuffer.GetAddressOf(),
		vertexErrorMessage.GetAddressOf()
	);
	if (FAILED(result)) {
		// If the shader failed to compile it should have writen something to the error message.
		if (vertexErrorMessage) {
			OutputShaderErrorMessage(vertexErrorMessage.Get(), hwnd, vsFilename);
		}
		// If there was nothing in the error message then it simply could not find the shader file itself.
		else {
			MessageBoxW(hwnd, vsFilename, L"Missing Shader File", MB_OK);
		}

		return false;
	}

	// Compile the pixel shader code.
	result = D3DCompileFromFile(
		psFilename,
		NULL,
		NULL,
		"TexturePixelShader",
		"ps_5_0",
		D3D10_SHADER_ENABLE_STRICTNESS,
		0,
		pixelShaderBuffer.GetAddressOf(),
		pixelErrorMessage.GetAddressOf()
	);
	if (FAILED(result)) {
		// If the shader failed to compile it should have writen something to the error message.
		if (pixelErrorMessage) {
			OutputShaderErrorMessage(pixelErrorMessage.Get(), hwnd, psFilename);
		}
		// If there was nothing in the error message then it simply could not find the file itself.
		else {
			MessageBoxW(hwnd, psFilename, L"Missing Shader File", MB_OK);
		}

		return false;
	}

	// Create the vertex shader from the buffer.
	result = device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, m_vertexShader.GetAddressOf());
	if (FAILED(result)) {
		return false;
	}

	// Create the pixel shader from the buffer.
	result = device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, m_pixelShader.GetAddressOf());
	if (FAILED(result)) {
		return false;
	}

	// Create the vertex input layout description.
	// This setup needs to match the VertexType stucture in the ModelClass and in the shader.
	D3D11_INPUT_ELEMENT_DESC polygonLayout[2];
	polygonLayout[0].SemanticName = "POSITION";
	polygonLayout[0].SemanticIndex = 0;
	polygonLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polygonLayout[0].InputSlot = 0;
	polygonLayout[0].AlignedByteOffset = 0;
	polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[0].InstanceDataStepRate = 0;

	polygonLayout[1].SemanticName = "TEXCOORD";
	polygonLayout[1].SemanticIndex = 0;
	polygonLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	polygonLayout[1].InputSlot = 0;
	polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[1].InstanceDataStepRate = 0;

	// Get a count of the elements in the layout.
	unsigned int numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

	// Create the vertex input layout.
	result = device->CreateInputLayout(
		polygonLayout,
		numElements,
		vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(),
		m_layout.GetAddressOf()
	);
	if (FAILED(result)) {
		return false;
	}

	// Setup the description of the dynamic matrix constant buffer that is in the vertex shader.
	D3D11_BUFFER_DESC matrixBufferDesc;
	matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	matrixBufferDesc.ByteWidth = sizeof(MatrixBufferType);
	matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	matrixBufferDesc.MiscFlags = 0;
	matrixBufferDesc.StructureByteStride = 0;

	// Create the constant buffer pointer so we can access the vertex shader constant buffer from within this class.
	result = device->CreateBuffer(&matrixBufferDesc, NULL, m_matrixBuffer.GetAddressOf());
	if (FAILED(result)) {
		return false;
	}

	// Create a texture sampler state description.
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.BorderColor[0] = 0;
	samplerDesc.BorderColor[1] = 0;
	samplerDesc.BorderColor[2] = 0;
	samplerDesc.BorderColor[3] = 0;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create the texture sampler state.
	result = device->CreateSamplerState(&samplerDesc, m_sampleState.GetAddressOf());
	if (FAILED(result)) {
		return false;
	}

	return true;
}

void TextureShaderClass::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, LPCWSTR shaderFilename) {
	char* compileErrors;
	unsigned long long bufferSize, i;
	ofstream fout;

	// Get a pointer to the error message text buffer.
	compileErrors = (char*)(errorMessage->GetBufferPointer());

	// Get the length of the message.
	bufferSize = errorMessage->GetBufferSize();

	// Open a file to write the error message to.
	fout.open("shader-error.txt");

	// Write out the error message.
	for (i = 0; i < bufferSize; i++) {
		fout << compileErrors[i];
	}

	// Close the file.
	fout.close();

	// Release the error message.
	errorMessage->Release();
	errorMessage = 0;

	// Pop a message up on the screen to notify the user to check the text file for compile errors.
	MessageBoxW(hwnd, L"Error compiling shader.  Check shader-error.txt for message.", shaderFilename, MB_OK);

	return;
}

bool TextureShaderClass::SetShaderParameters(ID3D11DeviceContext* deviceContext, XMMATRIX worldMatrix, XMMATRIX viewMatrix, XMMATRIX projectionMatrix, ID3D11ShaderResourceView* texture) {
	HRESULT result;

	unsigned int bufferNumber;

	// Transpose the matrices to prepare them for the shader.
	worldMatrix = XMMatrixTranspose(worldMatrix);
	viewMatrix = XMMatrixTranspose(viewMatrix);
	projectionMatrix = XMMatrixTranspose(projectionMatrix);

	// Lock the constant buffer so it can be written to.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	result = deviceContext->Map(m_matrixBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result)) {
		return false;
	}

	// Get a pointer to the data in the constant buffer.
	MatrixBufferType* dataPtr = (MatrixBufferType*)mappedResource.pData;

	// Copy the matrices into the constant buffer.
	dataPtr->world = worldMatrix;
	dataPtr->view = viewMatrix;
	dataPtr->projection = projectionMatrix;

	// Unlock the constant buffer.
	deviceContext->Unmap(m_matrixBuffer.Get(), 0);

	// Set the position of the constant buffer in the vertex shader.
	bufferNumber = 0;

	// Finanly set the constant buffer in the vertex shader with the updated values.
	deviceContext->VSSetConstantBuffers(bufferNumber, 1, m_matrixBuffer.GetAddressOf());

	// Set shader texture resource in the pixel shader.
	deviceContext->PSSetShaderResources(0, 1, &texture);

	return true;
}

void TextureShaderClass::RenderShader(ID3D11DeviceContext* deviceContext, int indexCount) {
	// Set the vertex input layout.
	deviceContext->IASetInputLayout(m_layout.Get());

	// Set the vertex and pixel shaders that will be used to render this triangle.
	deviceContext->VSSetShader(m_vertexShader.Get(), NULL, 0);
	deviceContext->PSSetShader(m_pixelShader.Get(), NULL, 0);

	// Set the sampler state in the pixel shader.
	deviceContext->PSSetSamplers(0, 1, &m_sampleState);

	// Render the triangle.
	deviceContext->DrawIndexed(indexCount, 0, 0);

	return;
}