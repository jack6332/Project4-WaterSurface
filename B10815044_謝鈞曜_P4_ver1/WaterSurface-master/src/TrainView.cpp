/************************************************************************
     File:        TrainView.cpp

     Author:     
                  Michael Gleicher, gleicher@cs.wisc.edu

     Modifier
                  Yu-Chi Lai, yu-chi@cs.wisc.edu
     
     Comment:     
						The TrainView is the window that actually shows the 
						train. Its a
						GL display canvas (Fl_Gl_Window).  It is held within 
						a TrainWindow
						that is the outer window with all the widgets. 
						The TrainView needs 
						to be aware of the window - since it might need to 
						check the widgets to see how to draw

	  Note:        we need to have pointers to this, but maybe not know 
						about it (beware circular references)

     Platform:    Visio Studio.Net 2003/2005

*************************************************************************/

#include <iostream>
#include <Fl/fl.h>

// we will need OpenGL, and OpenGL needs windows.h
#include <windows.h>
//#include "GL/gl.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GL/glu.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "TrainView.H"
#include "TrainWindow.H"
#include "Utilities/3DUtils.H"

#define STB_IMAGE_IMPLEMENTATION
//#define dcube
#include"RenderUtilities/model.h"


unsigned int loadCubemap(vector<std::string> faces)
{
	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height, nrChannels;
	for (unsigned int i = 0; i < faces.size(); i++)
	{
		unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
		if (data)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
				0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
			);
			stbi_image_free(data);
		}
		else
		{
			std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
			stbi_image_free(data);
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	
	return textureID;
}
//************************************************************************
//
// * Constructor to set up the GL window
//========================================================================
TrainView::
TrainView(int x, int y, int w, int h, const char* l) 
	: Fl_Gl_Window(x,y,w,h,l)
//========================================================================
{
	mode( FL_RGB|FL_ALPHA|FL_DOUBLE | FL_STENCIL );

	resetArcball();
}

//************************************************************************
//
// * Reset the camera to look at the world
//========================================================================
void TrainView::
resetArcball()
//========================================================================
{
	// Set up the camera to look at the world
	// these parameters might seem magical, and they kindof are
	// a little trial and error goes a long way
	arcball.setup(this, 40, 250, .2f, .4f, 0);
}

//************************************************************************
//
// * FlTk Event handler for the window
//########################################################################
// TODO: 
//       if you want to make the train respond to other events 
//       (like key presses), you might want to hack this.
//########################################################################
//========================================================================
int TrainView::handle(int event)
{
	// see if the ArcBall will handle the event - if it does, 
	// then we're done
	// note: the arcball only gets the event if we're in world view
	if (tw->worldCam->value())
		if (arcball.handle(event)) 
			return 1;

	// remember what button was used
	static int last_push;

	switch(event) {
		// Mouse button being pushed event
		case FL_PUSH:
			last_push = Fl::event_button();
			// if the left button be pushed is left mouse button
			if (last_push == FL_LEFT_MOUSE  ) {
				doPick();
				damage(1);
				return 1;
			};
			break;

	   // Mouse button release event
		case FL_RELEASE: // button release
			damage(1);
			last_push = 0;
			return 1;

		// Mouse button drag event
		case FL_DRAG:

			// Compute the new control point position
			if ((last_push == FL_LEFT_MOUSE) && (selectedCube >= 0)) {
				ControlPoint* cp = &m_pTrack->points[selectedCube];

				double r1x, r1y, r1z, r2x, r2y, r2z;
				getMouseLine(r1x, r1y, r1z, r2x, r2y, r2z);

				double rx, ry, rz;
				mousePoleGo(r1x, r1y, r1z, r2x, r2y, r2z, 
								static_cast<double>(cp->pos.x), 
								static_cast<double>(cp->pos.y),
								static_cast<double>(cp->pos.z),
								rx, ry, rz,
								(Fl::event_state() & FL_CTRL) != 0);

				cp->pos.x = (float) rx;
				cp->pos.y = (float) ry;
				cp->pos.z = (float) rz;
				damage(1);
			}
			break;

		// in order to get keyboard events, we need to accept focus
		case FL_FOCUS:
			return 1;

		// every time the mouse enters this window, aggressively take focus
		case FL_ENTER:	
			focus(this);
			break;

		case FL_KEYBOARD:
		 		int k = Fl::event_key();
				int ks = Fl::event_state();
				if (k == 'p') {
					// Print out the selected control point information
					if (selectedCube >= 0) 
						printf("Selected(%d) (%g %g %g) (%g %g %g)\n",
								 selectedCube,
								 m_pTrack->points[selectedCube].pos.x,
								 m_pTrack->points[selectedCube].pos.y,
								 m_pTrack->points[selectedCube].pos.z,
								 m_pTrack->points[selectedCube].orient.x,
								 m_pTrack->points[selectedCube].orient.y,
								 m_pTrack->points[selectedCube].orient.z);
					else
						printf("Nothing Selected\n");

					return 1;
				};
				break;
	}

	return Fl_Gl_Window::handle(event);
}

//************************************************************************
//
// * this is the code that actually draws the window
//   it puts a lot of the work into other routines to simplify things
//========================================================================
void TrainView::draw()
{

	//*********************************************************************
	//
	// * Set up basic opengl informaiton
	//
	//**********************************************************************
	//initialized glad
	if (gladLoadGL())
	{
		//initiailize VAO, VBO, Shader...
		if (!this->tile) {
			this->tile = new
				Shader(
					"../WaterSurface-master/src/shaders/load_model.vert",
					nullptr, nullptr, nullptr,
					"../WaterSurface-master/src/shaders/load_model.frag");
		}
		if (!this->wave_shader) {
			this->wave_shader = new
				Shader(
					"../WaterSurface-master/src/shaders/wave.vert",
					nullptr, nullptr, nullptr,
					"../WaterSurface-master/src/shaders/wave.frag");
		}
		if (!this->skybox) {
			this->skybox = new
				Shader(
					"../WaterSurface-master/src/shaders/skybox.vert",
					nullptr, nullptr, nullptr,
					"../WaterSurface-master/src/shaders/skybox.frag");

			float skyboxVertices[] = {       
				-1.0f,  1.0f, -1.0f,
				-1.0f, -1.0f, -1.0f,
				 1.0f, -1.0f, -1.0f,
				 1.0f, -1.0f, -1.0f,
				 1.0f,  1.0f, -1.0f,
				-1.0f,  1.0f, -1.0f,

				-1.0f, -1.0f,  1.0f,
				-1.0f, -1.0f, -1.0f,
				-1.0f,  1.0f, -1.0f,
				-1.0f,  1.0f, -1.0f,
				-1.0f,  1.0f,  1.0f,
				-1.0f, -1.0f,  1.0f,

				 1.0f, -1.0f, -1.0f,
				 1.0f, -1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f, -1.0f,
				 1.0f, -1.0f, -1.0f,

				-1.0f, -1.0f,  1.0f,
				-1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f, -1.0f,  1.0f,
				-1.0f, -1.0f,  1.0f,

				-1.0f,  1.0f, -1.0f,
				 1.0f,  1.0f, -1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				-1.0f,  1.0f,  1.0f,
				-1.0f,  1.0f, -1.0f,

				-1.0f, -1.0f, -1.0f,
				-1.0f, -1.0f,  1.0f,
				 1.0f, -1.0f, -1.0f,
				 1.0f, -1.0f, -1.0f,
				-1.0f, -1.0f,  1.0f,
				 1.0f, -1.0f,  1.0f
			};
			glGenVertexArrays(1, &skyboxVAO);
			glGenBuffers(1, &skyboxVBO);
			glBindVertexArray(skyboxVAO);
			glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			vector<std::string> faces
			{
				"../skybox/right.jpg",
				"../skybox/left.jpg",
				"../skybox/top.jpg",
				"../skybox/bottom.jpg",
				"../skybox/front.jpg",
				"../skybox/back.jpg"

			};
			cubemapTexture = loadCubemap(faces);
		}
		if (!this->screenShader) {
			this->screenShader = new
				Shader(
					"../WaterSurface-master/src/shaders/screenShader.vert",
					nullptr, nullptr, nullptr,
					"../WaterSurface-master/src/shaders/screenShader.frag");

			float quadVertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
			  // positions   // texCoords
			  -1.0f,  1.0f,  0.0f, 1.0f,
			  -1.0f, -1.0f,  0.0f, 0.0f,
			   1.0f, -1.0f,  1.0f, 0.0f,

			  -1.0f,  1.0f,  0.0f, 1.0f,
			   1.0f, -1.0f,  1.0f, 0.0f,
			   1.0f,  1.0f,  1.0f, 1.0f
					};
			// screen quad VAO
			glGenVertexArrays(1, &quadVAO);
			glGenBuffers(1, &quadVBO);
			glBindVertexArray(quadVAO);
			glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
			glBindVertexArray(0);
			// framebuffer configuration
			// -------------------------
			glGenFramebuffers(1, &framebuffer);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
			// create a color attachment texture
			glGenTextures(1, &textureColorbuffer);
			glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w(), h(), 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);
			// create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
			unsigned int rbo;
			glGenRenderbuffers(1, &rbo);
			glBindRenderbuffer(GL_RENDERBUFFER, rbo);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w(), h()); // use a single renderbuffer object for both a depth AND stencil buffer.
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo); // now actually attach it
			// now that we actually created the framebuffer and added all attachments we want to check if it is actually complete now
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << endl;
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		if (!this->interactive_frame) {
			this->interactive_frame = new
				Shader(
					"../WaterSurface-master/src/shaders/interact_frame.vert",
					nullptr, nullptr, nullptr,
					"../WaterSurface-master/src/shaders/interact_frame.frag");

			float quadVertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
			  // positions   // texCoords
			  -1.0f,  1.0f,  0.0f, 1.0f,
			  -1.0f, -1.0f,  0.0f, 0.0f,
			   1.0f, -1.0f,  1.0f, 0.0f,

			  -1.0f,  1.0f,  0.0f, 1.0f,
			   1.0f, -1.0f,  1.0f, 0.0f,
			   1.0f,  1.0f,  1.0f, 1.0f
			};
			// screen quad VAO
			glGenVertexArrays(1, &quadVAO1);
			glGenBuffers(1, &quadVBO1);
			glBindVertexArray(quadVAO1);
			glBindBuffer(GL_ARRAY_BUFFER, quadVBO1);
			glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
			// framebuffer configuration
			// -------------------------
			glGenFramebuffers(1, &framebuffer1);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer1);
			// create a color attachment texture
			glGenTextures(1, &textureColorbuffer1);
			glBindTexture(GL_TEXTURE_2D, textureColorbuffer1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w(), h(), 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer1, 0);
			// now that we actually created the framebuffer and added all attachments we want to check if it is actually complete now
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << endl;
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		if (!this->commom_matrices) {
			this->commom_matrices = new UBO();
			this->commom_matrices->size = 2 * sizeof(glm::mat4);
			glGenBuffers(1, &this->commom_matrices->ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, this->commom_matrices->ubo);
			glBufferData(GL_UNIFORM_BUFFER, this->commom_matrices->size, NULL, GL_STATIC_DRAW);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}
			

		if (!wave_model) {
			wave_model = new Model("../wave/wave.obj");
		}

		if (!tile_model) {
			tile_model = new Model("../tile/tile.obj");
			tile_texture = new Texture2D("../tile/tiles.jpg");
		}

		if (!this->device) {
			//Tutorial: https://ffainelli.github.io/openal-example/
			this->device = alcOpenDevice(NULL);
			if (!this->device)
				puts("ERROR::NO_AUDIO_DEVICE");

			ALboolean enumeration = alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");
			if (enumeration == AL_FALSE)
				puts("Enumeration not supported");
			else
				puts("Enumeration supported");

			this->context = alcCreateContext(this->device, NULL);
			if (!alcMakeContextCurrent(context))
				puts("Failed to make context current");

			this->source_pos = glm::vec3(0.0f, 5.0f, 0.0f);

			ALfloat listenerOri[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };
			alListener3f(AL_POSITION, source_pos.x, source_pos.y, source_pos.z);
			alListener3f(AL_VELOCITY, 0, 0, 0);
			alListenerfv(AL_ORIENTATION, listenerOri);

			alGenSources((ALuint)1, &this->source);
			alSourcef(this->source, AL_PITCH, 1);
			alSourcef(this->source, AL_GAIN, 1.0f);
			alSource3f(this->source, AL_POSITION, source_pos.x, source_pos.y, source_pos.z);
			alSource3f(this->source, AL_VELOCITY, 0, 0, 0);
			alSourcei(this->source, AL_LOOPING, AL_TRUE);

			alGenBuffers((ALuint)1, &this->buffer);

			ALsizei size, freq;
			ALenum format;
			ALvoid* data;
			ALboolean loop = AL_TRUE;


			alutLoadWAVFile((ALbyte*)"../WaterSurface/Audios/bounce.wav", &format, &data, &size, &freq, &loop);
			alBufferData(this->buffer, format, data, size, freq);
			alSourcei(this->source, AL_BUFFER, this->buffer);

			if (format == AL_FORMAT_STEREO16 || format == AL_FORMAT_STEREO8)
				puts("TYPE::STEREO");
			else if (format == AL_FORMAT_MONO16 || format == AL_FORMAT_MONO8)
				puts("TYPE::MONO");

			alSourcePlay(this->source);


			alDeleteSources(1, &source);
			alDeleteBuffers(1, &buffer);
			device = alcGetContextsDevice(context);
			alcMakeContextCurrent(NULL);
			alcDestroyContext(context);
			alcCloseDevice(device);
		}
	}
	else
		throw std::runtime_error("Could not initialize GLAD!");

	// Set up the view port
	glViewport(0, 0, w(), h());

	// clear the window, be sure to clear the Z-Buffer too
	glClearColor(0, 0, .3f, 0);		// background should be blue

	// we need to clear out the stencil buffer since we'll use
	// it for shadows
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glEnable(GL_DEPTH);

	// Blayne prefers GL_DIFFUSE
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

	// prepare for projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	setProjection();		// put the code to set up matrices here

	//######################################################################
	// TODO: 
	// you might want to set the lighting up differently. if you do, 
	// we need to set up the lights AFTER setting up the projection
	//######################################################################
	// enable the lighting
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	// top view only needs one light
	if (tw->topCam->value()) {
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
	}
	else {
		glEnable(GL_LIGHT1);
		glEnable(GL_LIGHT2);
	}

	//*********************************************************************
	//
	// * set the light parameters
	//
	//**********************************************************************
	//GLfloat lightPosition1[] = { 0,1,1,0 }; // {50, 200.0, 50, 1.0};
	//GLfloat lightPosition2[] = { 1, 0, 0, 0 };
	//GLfloat lightPosition3[] = { 0, -1, 0, 0 };
	//GLfloat yellowLight[] = { 0.5f, 0.5f, .1f, 1.0 };
	//GLfloat whiteLight[] = { 1.0f, 1.0f, 1.0f, 1.0 };
	//GLfloat blueLight[] = { .1f,.1f,.3f,1.0 };
	//GLfloat grayLight[] = { .3f, .3f, .3f, 1.0 };

	//glLightfv(GL_LIGHT0, GL_POSITION, lightPosition1);
	//glLightfv(GL_LIGHT0, GL_DIFFUSE, whiteLight);
	//glLightfv(GL_LIGHT0, GL_AMBIENT, grayLight);

	//glLightfv(GL_LIGHT1, GL_POSITION, lightPosition2);
	//glLightfv(GL_LIGHT1, GL_DIFFUSE, yellowLight);

	//glLightfv(GL_LIGHT2, GL_POSITION, lightPosition3);
	//glLightfv(GL_LIGHT2, GL_DIFFUSE, blueLight);

	// set linstener position 
	/*if (selectedCube >= 0)
		alListener3f(AL_POSITION,
			m_pTrack->points[selectedCube].pos.x,
			m_pTrack->points[selectedCube].pos.y,
			m_pTrack->points[selectedCube].pos.z);
	else
		alListener3f(AL_POSITION,
			this->source_pos.x,
			this->source_pos.y,
			this->source_pos.z);*/


	//*********************************************************************
	// now draw the ground plane
	//*********************************************************************
	// set to opengl fixed pipeline(use opengl 1.x draw function)
	//glUseProgram(0);

	/*setupFloor();
	glDisable(GL_LIGHTING);
	drawFloor(200, 10);*/


	//*********************************************************************
	// now draw the object and we need to do it twice
	// once for real, and then once for shadows
	//*********************************************************************
	/*glEnable(GL_LIGHTING);
	setupObjects();*/

	drawStuff();

	// this time drawing is for shadows (except for top view)
	if (!tw->topCam->value()) {
		setupShadows();
		drawStuff(true);
		unsetupShadows();
	}
	//Frame buffer
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glEnable(GL_DEPTH_TEST); // enable depth testing (is disabled for rendering screen-space quad)
	// make sure we clear the framebuffer's content
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (tw->waveBrowser->value()==2) {
		if (first) {
			for (int i = 0; i < 200; i++) {
				string name = to_string(i);
				if (name.size() == 1) {
					name = "00" + name;
				}
				else if (name.size() == 2) {
					name = "0" + name;
				}
				name = name + ".png";
				height_id.push_back(TextureFromFile(name.c_str(), "../height_map"));
			}
			first = false;
		}
		wave_model->meshes[0].textures[0].id = height_id[height_index];
		height_index = ++height_index % 200;
	}
	wave_shader->Use();


	
	glGetFloatv(GL_PROJECTION_MATRIX, Projection);
	glGetFloatv(GL_MODELVIEW_MATRIX, View);


	glm::mat4 skybox_View = glm::mat4(glm::mat3(glm::make_mat4(View)));
	glm::mat4 viewMatrix = glm::inverse(glm::make_mat4(View));
	glm::vec3 viewPos(viewMatrix[3][0], viewMatrix[3][1], viewMatrix[3][2]);

	//transformation matrix
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::scale(model, glm::vec3(scale, scale, scale));
	glUniform3f(glGetUniformLocation(wave_shader->Program, "viewPos"), viewPos.x, viewPos.y, viewPos.z);
	/*Sine Wave*/
	glUniform1f(glGetUniformLocation(wave_shader->Program, "amplitude"),tw->WaveAmplitude->value());
	glUniform1f(glGetUniformLocation(wave_shader->Program, "frequency"), tw->WaveScale->value());
	glUniform1f(glGetUniformLocation(wave_shader->Program, "t"), tw->wave_t);
	glUniform1f(glGetUniformLocation(wave_shader->Program, "reflect_enable"), tw->reflect);
	glUniform1f(glGetUniformLocation(wave_shader->Program, "refract_enable"), tw->refract);
	glUniform1i(glGetUniformLocation(wave_shader->Program, "wave_mode"), tw->waveBrowser->value());

	glUniformMatrix4fv(glGetUniformLocation(wave_shader->Program, "proj_matrix"), 1, GL_FALSE, Projection);
	glUniformMatrix4fv(glGetUniformLocation(wave_shader->Program, "view_matrix"), 1, GL_FALSE, View);
	glUniformMatrix4fv(glGetUniformLocation(wave_shader->Program, "model_matrix"), 1, GL_FALSE, &model[0][0]);

	glUniform2f(glGetUniformLocation(wave_shader->Program, "uv_center"), uv_center.x, uv_center.y);
	glUniform1f(glGetUniformLocation(wave_shader->Program, "uv_t"), uv_t);
	wave_model->Draw(*wave_shader);

	
	/*Lighting*/
	//�}��
	glUniform1f(glGetUniformLocation(wave_shader->Program, "direct_enable"), tw->direct);
	glUniform1f(glGetUniformLocation(wave_shader->Program, "point_enable"), tw->point);
	glUniform1f(glGetUniformLocation(wave_shader->Program, "spot_enable"), tw->spot);
	//Direction light
	glUniform3f(glGetUniformLocation(wave_shader->Program, "dirLight.direction"), 100.0f, 1.0f, 0.0f);
	glUniform3f(glGetUniformLocation(wave_shader->Program, "dirLight.ambient"), 0.6f, 0.6f, 0.6f);
	glUniform3f(glGetUniformLocation(wave_shader->Program, "dirLight.diffuse"), 0.4f, 0.4f, 0.4f);
	glUniform3f(glGetUniformLocation(wave_shader->Program, "dirLight.specular"), 1.0f, 1.0f, 1.0f);
	//point light
	glUniform3f(glGetUniformLocation(wave_shader->Program, "pointLights.position"), 0, 5, 0);
	glUniform3f(glGetUniformLocation(wave_shader->Program, "pointLights.direction"), 0.0f, 1.5f, 0.0f);
	glUniform3f(glGetUniformLocation(wave_shader->Program, "pointLights.ambient"), 0.1f, 0.1f, 0.1f);
	glUniform3f(glGetUniformLocation(wave_shader->Program, "pointLights.diffuse"), 0.4f, 0.4f, 0.4f);
	glUniform3f(glGetUniformLocation(wave_shader->Program, "pointLights.specular"), 1.0f, 1.0f, 1.0f);
	glUniform1f(glGetUniformLocation(wave_shader->Program, "pointLights.constant"), 1.0f);
	glUniform1f(glGetUniformLocation(wave_shader->Program, "pointLights.linear"), 0.09f);
	glUniform1f(glGetUniformLocation(wave_shader->Program, "pointLights.quadratic"), 0.032f);
	//spot light
	glUniform3f(glGetUniformLocation(wave_shader->Program, "spotLight.position"), 0, 5, 0);
	glUniform3f(glGetUniformLocation(wave_shader->Program, "spotLight.direction"), viewPos[0], viewPos[1], viewPos[2]);
	glUniform3f(glGetUniformLocation(wave_shader->Program, "spotLight.ambient"), 0.1f, 0.1f, 0.1f);
	glUniform3f(glGetUniformLocation(wave_shader->Program, "spotLight.diffuse"), 1.0f, 1.0f, 1.0f);
	glUniform3f(glGetUniformLocation(wave_shader->Program, "spotLight.specular"), 1.0f, 1.0f, 1.0f);
	glUniform1f(glGetUniformLocation(wave_shader->Program, "spotLight.constant"), 1.0f);
	glUniform1f(glGetUniformLocation(wave_shader->Program, "spotLight.linear"), 0.09f);
	glUniform1f(glGetUniformLocation(wave_shader->Program, "spotLight.cutOff"), 0.032f);
	glUniform1f(glGetUniformLocation(wave_shader->Program, "spotLight.quadratic"), glm::cos(glm::radians(12.5f)));
	glUniform1f(glGetUniformLocation(wave_shader->Program, "spotLight.outerCutOff"), glm::cos(glm::radians(15.0f)));
	//material
	glUniform1f(glGetUniformLocation(wave_shader->Program, "material.diffuse"), 0.0f);
	glUniform1f(glGetUniformLocation(wave_shader->Program, "material.specular"), 1.0f);
	glUniform1f(glGetUniformLocation(wave_shader->Program, "material.shininess"), 32.0f);

	/*Sky box*/
	glDepthFunc(GL_LEQUAL);
	skybox->Use();
	glUniformMatrix4fv(glGetUniformLocation(skybox->Program, "proj_matrix"), 1, GL_FALSE, Projection);
	glUniformMatrix4fv(glGetUniformLocation(skybox->Program, "model_matrix"), 1, GL_FALSE, &skybox_View[0][0]);
	// skybox cube
	glBindVertexArray(skyboxVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
	glDepthFunc(GL_LESS); 

	tile->Use();
	glUniformMatrix4fv(glGetUniformLocation(tile->Program, "proj_matrix"), 1, GL_FALSE, Projection);
	glUniformMatrix4fv(glGetUniformLocation(tile->Program, "view_matrix"), 1, GL_FALSE, View);
	glUniformMatrix4fv(glGetUniformLocation(tile->Program, "model_matrix"), 1, GL_FALSE, &model[0][0]);
	glUniform1i(glGetUniformLocation(tile->Program, "texture_d"), 11);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);

	tile_texture->bind(11);
	tile_model->Draw(*tile);
	tile_texture->unbind(11);
	glDisable(GL_CULL_FACE);

	// now bind back to default framebuffer and draw a quad plane with the attached framebuffer color texture
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
	// clear all relevant buffers
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessary actually, since we won't be able to see behind the quad anyways)
	glClear(GL_COLOR_BUFFER_BIT);
	this->screenShader->Use();
	glUniform1f(glGetUniformLocation(screenShader->Program, "pixel_enable"), tw->direct);
	glUniform1f(glGetUniformLocation(screenShader->Program, "offset_enable"), tw->point);
	glUniform1f(glGetUniformLocation(screenShader->Program, "other_enable"), tw->spot);
	glUniform1f(glGetUniformLocation(screenShader->Program, "rt_w"), w());
	glUniform1f(glGetUniformLocation(screenShader->Program, "rt_h"), h());
	glBindVertexArray(quadVAO);
	glActiveTexture(GL_TEXTURE12);
	glBindTexture(GL_TEXTURE_2D, textureColorbuffer);	// use the color attachment texture as the texture of the quad plane
	glUniform1i(glGetUniformLocation(screenShader->Program, "screenTexture"), 12);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glActiveTexture(GL_TEXTURE12);
	glBindTexture(GL_TEXTURE_2D, 0);
}

//************************************************************************
//
// * This sets up both the Projection and the View matrices
//   HOWEVER: it doesn't clear the projection first (the caller handles
//   that) - its important for picking
//========================================================================
void TrainView::
setProjection()
//========================================================================
{
	// Compute the aspect ratio (we'll need it)
	float aspect = static_cast<float>(w()) / static_cast<float>(h());

	// Check whether we use the world camp
	if (tw->worldCam->value())
		arcball.setProjection(false);
	// Or we use the top cam
	else if (tw->topCam->value()) {
		float wi, he;
		if (aspect >= 1) {
			wi = 110;
			he = wi / aspect;
		} 
		else {
			he = 110;
			wi = he * aspect;
		}

		// Set up the top camera drop mode to be orthogonal and set
		// up proper projection matrix
		glMatrixMode(GL_PROJECTION);
		glOrtho(-wi, wi, -he, he, 200, -200);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(-90,1,0,0);
	} 
	// Or do the train view or other view here
	//####################################################################
	// TODO: 
	// put code for train view projection here!	
	//####################################################################
	else {
#ifdef EXAMPLE_SOLUTION
		trainCamView(this,aspect);
#endif
	}
}

//************************************************************************
//
// * this draws all of the stuff in the world
//
//	NOTE: if you're drawing shadows, DO NOT set colors (otherwise, you get 
//       colored shadows). this gets called twice per draw 
//       -- once for the objects, once for the shadows
//########################################################################
// TODO: 
// if you have other objects in the world, make sure to draw them
//########################################################################
//========================================================================
void TrainView::drawStuff(bool doingShadows)
{
	// Draw the control points
	// don't draw the control points if you're driving 
	// (otherwise you get sea-sick as you drive through them)
	if (!tw->trainCam->value()) {
		for(size_t i=0; i<m_pTrack->points.size(); ++i) {
			if (!doingShadows) {
				if ( ((int) i) != selectedCube)
					glColor3ub(240, 60, 60);
				else
					glColor3ub(240, 240, 30);
			}
			m_pTrack->points[i].draw();
		}
	}
	// draw the track
	//####################################################################
	// TODO: 
	// call your own track drawing code
	//####################################################################

#ifdef EXAMPLE_SOLUTION
	drawTrack(this, doingShadows);
#endif

	// draw the train
	//####################################################################
	// TODO: 
	//	call your own train drawing code
	//####################################################################
#ifdef EXAMPLE_SOLUTION
	// don't draw the train if you're looking out the front window
	if (!tw->trainCam->value())
		drawTrain(this, doingShadows);
#endif
}

// 
//************************************************************************
//
// * this tries to see which control point is under the mouse
//	  (for when the mouse is clicked)
//		it uses OpenGL picking - which is always a trick
//########################################################################
// TODO: 
//		if you want to pick things other than control points, or you
//		changed how control points are drawn, you might need to change this
//########################################################################
//========================================================================
void TrainView::
doPick()
//========================================================================
{
	// since we'll need to do some GL stuff so we make this window as 
	// active window
	//make_current();		

	//// where is the mouse?
	//int mx = Fl::event_x(); 
	//int my = Fl::event_y();

	//// get the viewport - most reliable way to turn mouse coords into GL coords
	//int viewport[4];
	//glGetIntegerv(GL_VIEWPORT, viewport);

	//// Set up the pick matrix on the stack - remember, FlTk is
	//// upside down!
	//glMatrixMode(GL_PROJECTION);
	//glLoadIdentity ();
	//gluPickMatrix((double)mx, (double)(viewport[3]-my), 
	//					5, 5, viewport);

	//// now set up the projection
	//setProjection();

	//// now draw the objects - but really only see what we hit
	//GLuint buf[100];
	//glSelectBuffer(100,buf);
	//glRenderMode(GL_SELECT);
	//glInitNames();
	//glPushName(0);

	//// draw the cubes, loading the names as we go
	//for(size_t i=0; i<m_pTrack->points.size(); ++i) {
	//	glLoadName((GLuint) (i+1));
	//	m_pTrack->points[i].draw();
	//}

	//// go back to drawing mode, and see how picking did
	//int hits = glRenderMode(GL_RENDER);
	//if (hits) {
	//	// warning; this just grabs the first object hit - if there
	//	// are multiple objects, you really want to pick the closest
	//	// one - see the OpenGL manual 
	//	// remember: we load names that are one more than the index
	//	selectedCube = buf[3]-1;
	//} else // nothing hit, nothing selected
	//	selectedCube = -1;

	//printf("Selected Cube %d\n",selectedCube);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer1);
	glEnable(GL_DEPTH_TEST); // enable depth testing (is disabled for rendering screen-space quad)
	// make sure we clear the framebuffer's content
	glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	interactive_frame->Use();


	//transformation matrix
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::scale(glm::translate(model, glm::vec3(0, 10, 0)), glm::vec3(20, 20, 20));
	glUniformMatrix4fv(glGetUniformLocation(interactive_frame->Program, "proj_matrix"), 1, GL_FALSE, Projection);
	glUniformMatrix4fv(glGetUniformLocation(interactive_frame->Program, "view_matrix"), 1, GL_FALSE, View);
	glUniformMatrix4fv(glGetUniformLocation(interactive_frame->Program, "model_matrix"), 1, GL_FALSE, &model[0][0]);
	wave_model->Draw(*interactive_frame);

	//pick
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glm::vec3 uv;
	glReadPixels(Fl::event_x(),h()- Fl::event_y()-1, 1, 1, GL_RGB, GL_FLOAT, &uv[0]);

	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	if (uv.b != 1.0) {
		uv_center.x = uv.x;
		uv_center.y = uv.y;
		uv_t = tw->wave_t;
	}
}
void TrainView::setUBO()
{
	float wdt = this->pixel_w();
	float hgt = this->pixel_h();

	glm::mat4 view_matrix;
	glGetFloatv(GL_MODELVIEW_MATRIX, &view_matrix[0][0]);
	//HMatrix view_matrix; 
	//this->arcball.getMatrix(view_matrix);

	glm::mat4 projection_matrix;
	glGetFloatv(GL_PROJECTION_MATRIX, &projection_matrix[0][0]);
	//projection_matrix = glm::perspective(glm::radians(this->arcball.getFoV()), (GLfloat)wdt / (GLfloat)hgt, 0.01f, 1000.0f);

	glBindBuffer(GL_UNIFORM_BUFFER, this->commom_matrices->ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), &projection_matrix[0][0]);
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), &view_matrix[0][0]);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
