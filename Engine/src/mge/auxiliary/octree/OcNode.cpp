#include "OcNode.hpp"
#include "mge/config.hpp"
#include "mge/core/World.hpp"
#include "mge/core/Camera.hpp"

#include "Ball.hpp"

#include <vector>
#include <algorithm>

//maximum layers
int OcNode::_maxLayers = 2;

//nodes
std::vector<OcNode*> OcNode::emptyNodes;
std::vector<OcNode*> OcNode::inUseNodes;

//shader
ShaderProgram* OcNode::_shader = NULL;

GLuint OcNode::_aVertex = -1;
GLuint OcNode::_uMVP	= -1;
GLuint OcNode::_uColor	= -1;

//cube
GLuint OcNode::_bufferId   = -1;
GLuint OcNode::_bufferSize = 0;


OcNode::OcNode(OcNode* pParentNode, glm::vec3 offset)
{
	_parentNode = pParentNode;
	_position = offset;

	if (!pParentNode) 
	{
		_layer	= 0;
		_matrix = glm::mat4();
		_size	= 1;
	} else {
		_layer	= _parentNode->_layer + 1;
		_size	= _parentNode->_size / 2;

		_matrix	 = _parentNode->_matrix;
		_matrix *= glm::scale(glm::vec3(0.5f));
		_matrix *= glm::translate(offset);
		
		_position = glm::vec3(_matrix[3]);
	}

	emptyNodes.push_back(this);

	if (!_shader)
		initShader();

	if (_bufferId == -1)
		initCube();

	if (_layer < _maxLayers)
		spawnChildren();
}

void OcNode::spawnChildren()
{
	int count = 0;

	for (int x = 0; x < 2; x++)
		for (int y = 0; y < 2; y++)
			for (int z = 0; z < 2; z++)
			{
				glm::vec3 offset = glm::vec3(x, y, z) - glm::vec3(0.5f);
				_childNodes[count] = new OcNode(this, offset);
				count++;
			}
}

#pragma region Object hierarchy
///////////////////////////////
void OcNode::updateObject(Ball* ball)
{
	OcNode* parent = getParentNode(ball);
	OcNode* current = ball->getCurrentNode();
	
	if (parent == current)
		return;

	parent->addChild(ball);

	if (current)
		current->removeChild(ball);

	ball->setCurrentNode(parent);
}

OcNode* OcNode::getParentNode(Ball* ball)
{
	if (isInside(ball))
	{
		OcNode* potentialParent = searchChildren(ball);

		if (potentialParent)
			return potentialParent;

		return this;
	}

	if (_layer == 0)
		return this;

	return _parentNode->searchParent(ball, this);
}

OcNode* OcNode::searchParent(Ball* ball, OcNode* currentNode)
{
	if (_layer == 0)
		return this;

	if (isInside(ball))
	{
		for (int i = 0; i < 8; i++)
		{
			OcNode* childNode = _childNodes[i];

			if (childNode == currentNode)
				continue;

			OcNode* potentialParent = searchChildren(ball);

			if (potentialParent)
				return potentialParent;
		}
		return this;
	}

	return _parentNode->searchParent(ball, this);
}

OcNode* OcNode::searchChildren(Ball* ball)
{
	if (_layer == _maxLayers)
		return NULL;

	OcNode* parent = NULL;

	for (int i = 0; i < 8; i++)
	{
		OcNode* childNode = _childNodes[i];

		if (childNode->isInside(ball))
		{
			parent = childNode;
			OcNode* potentialParent = childNode->searchChildren(ball);

			if (potentialParent)
			{
				parent = potentialParent;
				break;
			}
		}
	}
	return parent;
}

void OcNode::addChild(Ball* ball)
{
	if (_childObjects.size() == 0)
	{
		inUseNodes.push_back(this);
		emptyNodes.erase(std::remove(emptyNodes.begin(), emptyNodes.end(), this), emptyNodes.end());
	}

	_childObjects.push_back(ball);
}

void OcNode::removeChild(Ball* ball)
{
	_childObjects.erase(std::remove(_childObjects.begin(), _childObjects.end(), ball), _childObjects.end());

	if (_childObjects.size() == 0)
	{
		emptyNodes.push_back(this);
		inUseNodes.erase(std::remove(inUseNodes.begin(), inUseNodes.end(), this), inUseNodes.end());
	}
}
#pragma endregion

#pragma region rendering
////////////////////////
void OcNode::render()
{
	_shader->use();

	glEnableVertexAttribArray(_aVertex);
	glBindBuffer(GL_ARRAY_BUFFER, _bufferId);
	glVertexAttribPointer(_aVertex, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	Camera* camera = World::getMainCamera();
	glm::mat4 vpMatrix = camera->getProjection() * glm::inverse(camera->getWorldTransform());

	for each (OcNode* node in emptyNodes)
		node->renderSelf(vpMatrix, glm::vec3(1, 1, 1));

	glClear(GL_DEPTH_BUFFER_BIT);

	for each (OcNode* node in inUseNodes)
		node->renderSelf(vpMatrix, glm::vec3(1, 0, 0));

	glDisableVertexAttribArray(_aVertex);
}

void OcNode::renderSelf(const glm::mat4& vpMatrix, glm::vec3 color)
{
	glm::mat4 mvpMatrix = vpMatrix * _matrix;

	glUniformMatrix4fv(_uMVP, 1, GL_FALSE, glm::value_ptr(mvpMatrix));
	glUniform3fv(_uColor, 1, glm::value_ptr(color));

	glDrawArrays(GL_LINES, 0, _bufferSize);
}

#pragma endregion

#pragma region initializing
///////////////////////////
void OcNode::initShader()
{
	std::cout << "initializing octree shader" << std::endl;

	_shader = new ShaderProgram();

	_shader->addShader(GL_VERTEX_SHADER, config::MGE_SHADER_PATH + "color.vs");
	_shader->addShader(GL_FRAGMENT_SHADER, config::MGE_SHADER_PATH + "color.fs");
	_shader->finalize();

	_aVertex = _shader->getAttribLocation("vertex");
	_uMVP	 = _shader->getUniformLocation("mvpMatrix");
	_uColor  = _shader->getUniformLocation("color");
}

void OcNode::initCube()
{
	std::cout << "initializing octree cube" << std::endl;

	glm::vec3 ind[] = {
		glm::vec3(-0.5f, -0.5f, -0.5f),	// 0
		glm::vec3(0.5f, -0.5f, -0.5f),	// 1
		glm::vec3(0.5f, 0.5f, -0.5f),	// 2
		glm::vec3(-0.5f, 0.5f, -0.5f),	// 3

		glm::vec3(-0.5f, -0.5f, 0.5f),	// 4
		glm::vec3(0.5f, -0.5f, 0.5f),	// 5
		glm::vec3(0.5f, 0.5f, 0.5f),	// 6
		glm::vec3(-0.5f, 0.5f, 0.5f)	// 7
	};

	std::vector<glm::vec3> lines; // contains the lines for the cube

	// front quad
	lines.push_back(ind[0]); lines.push_back(ind[1]);
	lines.push_back(ind[1]); lines.push_back(ind[2]);
	lines.push_back(ind[2]); lines.push_back(ind[3]);
	lines.push_back(ind[3]); lines.push_back(ind[0]);

	// rear quad
	lines.push_back(ind[4]); lines.push_back(ind[5]);
	lines.push_back(ind[5]); lines.push_back(ind[6]);
	lines.push_back(ind[6]); lines.push_back(ind[7]);
	lines.push_back(ind[7]); lines.push_back(ind[4]);

	// lines inbetween
	lines.push_back(ind[0]); lines.push_back(ind[4]);
	lines.push_back(ind[1]); lines.push_back(ind[5]);
	lines.push_back(ind[2]); lines.push_back(ind[6]);
	lines.push_back(ind[3]); lines.push_back(ind[7]);

	_bufferSize = lines.size();

	// put the lines into a buffer
	glGenBuffers(1, &_bufferId);
	glBindBuffer(GL_ARRAY_BUFFER, _bufferId);
	glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(lines), &lines[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}
#pragma endregion

#pragma region calculations
///////////////////////////
void OcNode::resolveCollision(Ball* ball)
{
	float cubeSize = _size / 2;

	float ballRadius  = ball->getRadius();
	glm::vec3 ballPos = ball->getLocalPosition();

	glm::vec3 min = _position - cubeSize + glm::vec3(ballRadius);
	glm::vec3 max = _position + cubeSize - glm::vec3(ballRadius);

	glm::vec3 planeNormal;
	glm::vec3 pointOnPlane;

	if		  (ballPos.x <= min.x) {
		planeNormal = glm::vec3(1, 0, 0);  pointOnPlane = _position - glm::vec3(cubeSize, 0, 0);
	} else if (ballPos.x >= max.x) {
		planeNormal = glm::vec3(-1, 0, 0); pointOnPlane = _position + glm::vec3(cubeSize, 0, 0);
	} else if (ballPos.y <= min.y) {
		planeNormal = glm::vec3(0, 1, 0);  pointOnPlane = _position - glm::vec3(0, cubeSize, 0);
	} else if (ballPos.y >= max.y) {
		planeNormal = glm::vec3(0, -1, 0); pointOnPlane = _position + glm::vec3(0, cubeSize, 0);
	} else if (ballPos.z <= min.z) {
		planeNormal = glm::vec3(0, 0, 1);  pointOnPlane = _position - glm::vec3(0, 0, cubeSize);
	} else if (ballPos.z >= max.z) {
		planeNormal = glm::vec3(0, 0, -1); pointOnPlane = _position + glm::vec3(0, 0, cubeSize);
	}
	else
		return;

	//float distA = glm::dot(pos - point, normal) - radius;
	//ball->setBack();
	//float distB = glm::dot(ball->getLocalPosition() - point, normal) - radius;

	ball->bounce(planeNormal);

}

bool OcNode::isInside(Ball* ball)
{
	float size = _size / 2;

	float ballRadius  = ball->getRadius();
	glm::vec3 ballPos = ball->getLocalPosition();

	glm::vec3 min = _position - size + glm::vec3(ballRadius);
	glm::vec3 max = _position + size - glm::vec3(ballRadius);

	if (min.x <= ballPos.x && ballPos.x <= max.x &&
		min.y <= ballPos.y && ballPos.y <= max.y &&
		min.z <= ballPos.z && ballPos.z <= max.z)
		return true;

	return false;
}
#pragma endregion


OcNode::~OcNode()
{
	if (_layer != _maxLayers)
		for (int i = 0; i < 8; i++)
			delete _childNodes[i];
}