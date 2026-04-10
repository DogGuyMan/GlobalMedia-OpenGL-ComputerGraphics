#include <sb7.h>
#include <shader.h>

#include <vmath.h>
#include <vector>
#include <memory>
#include <iostream>

namespace chapter6
{
	class ITranslatable
	{
	public:
		ITranslatable() = default;
		ITranslatable(ITranslatable &cpy) = delete;
		virtual ITranslatable operator=(ITranslatable &cpy) = delete;
		virtual ~ITranslatable();
		virtual vmath::vec3 GetPosition() const = 0;
		virtual void SetPosition(vmath::vec3 t) = 0;
		// 누적
		virtual void Translate(vmath::vec3 toward) = 0;
	};
	class IRotatable
	{
	public:
		IRotatable() = default;
		IRotatable(IRotatable &cpy) = delete;
		virtual IRotatable operator=(IRotatable &cpy) = delete;
		virtual ~IRotatable();
		virtual vmath::vec3 GetRotation() const = 0;
		virtual void SetRotation(vmath::vec3 euler) = 0;
		// 누적
	 	virtual void Rotate(float angle, vmath::vec3 axis) = 0;
	};

	class IScalable
	{
	public:
		IScalable() = default;
		IScalable(IScalable &cpy) = delete;
		virtual IScalable operator=(IScalable &cpy) = delete;
		virtual ~IScalable();
		virtual vmath::vec3 GetScale() const = 0;
		virtual void SetScale(vmath::vec3 s) = 0;
		// 누적
		virtual void Scale(vmath::vec3 adj) = 0;
	};

	class ITransformable : public ITranslatable, IRotatable, IScalable
	{
	public:
		ITransformable() = default;
		ITransformable(ITransformable &cpy) = delete;
		virtual ITransformable operator=(ITransformable &cpy) = delete;
		virtual ~ITransformable();
	};

	template <class T>
	class Transfomer
	{
	private:
		T *mOwner;
		vmath::mat4 mModelTransfomerMatrix;

	public:
		Transfomer(T *_owner) : mOwner(_owner),
					      mModelTransfomerMatrix(vmath::mat4::identity())
		{
		}
		~Transfomer()
		{
			mOwner = nullptr;
		}
		vmath::mat4 GetModelTransformerMatrix() const
		{
			return mModelTransfomerMatrix;
		}

		vmath::vec3 GetPosition() const
		{
			return vmath::vec3(
			    mModelTransfomerMatrix[4][0],
			    mModelTransfomerMatrix[4][1],
			    mModelTransfomerMatrix[4][2]);
		};

		void SetPosition(vmath::vec3 _t)
		{
			mModelTransfomerMatrix[4][0] = _t[0];
			mModelTransfomerMatrix[4][1] = _t[1];
			mModelTransfomerMatrix[4][2] = _t[2];
		};

		void Translate(vmath::vec3 toward)
		{
			mModelTransfomerMatrix += vmath::translate(toward);
		};

		// 롤 야우 피치 각을 어떻게 역으로 얻지?
		vmath::vec3 GetRotation() const
		{
			return vmath::vec3(0, 0, 0);
		};

		// 회전행렬은 강제로 덮어 씌울 수는 있겠다.
		void SetRotation(vmath::vec3 euler) {
			auto overrideRot = vmath::mat4::identity();
			overrideRot += vmath::rotate(euler[0], vmath::vec3(1.0, 0.0, 0.0));
			overrideRot += vmath::rotate(euler[1], vmath::vec3(0.0, 1.0, 0.0));
			overrideRot += vmath::rotate(euler[2], vmath::vec3(0.0, 0.0, 1.0));
			for(int i = 0; i < 3; i++)
				for(int j = 0; j < 3; j++)
					mModelTransfomerMatrix[i][j] = overrideRot[i][j];
		};

		void Rotate(float angle, vmath::vec3 axis)
		{
			mModelTransfomerMatrix += vmath::rotate(angle, axis);
		};

		vmath::vec3 GetScale() const {
			return vmath::vec3(
				mModelTransfomerMatrix[0][0],
				mModelTransfomerMatrix[1][1],
				mModelTransfomerMatrix[2][2]
			);
		};

		void SetScale(vmath::vec3 s) {
			mModelTransfomerMatrix[0][0] = s[0];
			mModelTransfomerMatrix[1][1] = s[1];
			mModelTransfomerMatrix[2][2] = s[2];
		};

		void Scale(vmath::vec3 adj) {
			mModelTransfomerMatrix += vmath::scale(adj);
		};
	};

	class Model : public ITransformable
	{
	protected:
		GLuint mVaoAddr;
		GLuint mVboAddr;
		std::vector<GLfloat> mModelData;
		Transfomer<Model> mTransformer;

	public:
		Model() : mTransformer(this)
		{
			glGenVertexArrays(1, &mVaoAddr);
			glBindVertexArray(mVaoAddr);

			glGenBuffers(1, &mVboAddr);
			glBindBuffer(GL_ARRAY_BUFFER, mVboAddr);
		}

		virtual ~Model()
		{
			glDeleteVertexArrays(1, &mVaoAddr);
		}

		virtual vmath::vec3 GetPosition() const override { return mTransformer.GetPosition(); }
		virtual void SetPosition(vmath::vec3 t) override {  mTransformer.SetPosition(t); }
		virtual void Translate(vmath::vec3 toward) override {  mTransformer.Translate(toward); }
		virtual vmath::vec3 GetRotation() const override {return mTransformer.GetRotation(); }
		virtual void SetRotation(vmath::vec3 euler) override { mTransformer.SetRotation(euler); }
		virtual void Rotate(float angle, vmath::vec3 axis) override { mTransformer.Rotate(angle,axis); }
		virtual vmath::vec3 GetScale() const override {return mTransformer.GetScale(); }
		virtual void SetScale(vmath::vec3 s) override { mTransformer.SetScale(s); }
		virtual void Scale(vmath::vec3 adj) override { mTransformer.Scale(adj); }
		
		vmath::mat4 GetModelTransformerMatrix() const
		{
			return mTransformer.GetModelTransformerMatrix();
		}

		virtual const std::vector<GLfloat>& GetModelData() const {
			return mModelData;
		}
	};

	class Cube : public Model
	{
	private:
		vmath::vec4 mOffset;
		const vmath::vec4 mBaseVertices[2][2][2] = {
		    {{
			 vmath::vec4(0.0, 0.0, 0.0, 1.0),
			 vmath::vec4(1.0, 0.0, 0.0, 1.0),
		     },
		     {
			 vmath::vec4(0.0, 1.0, 0.0, 1.0),
			 vmath::vec4(1.0, 1.0, 0.0, 1.0),
		     }},
		    {{
			 vmath::vec4(0.0, 0.0, 1.0, 1.0),
			 vmath::vec4(1.0, 0.0, 1.0, 1.0),
		     },
		     {
			 vmath::vec4(0.0, 0.0, 1.0, 1.0),
			 vmath::vec4(0.0, 1.0, 1.0, 1.0),
		     }}};

		const vmath::vec4 mBaseColors[6] = {
		    vmath::vec4(1.0, 0.0, 0.0, 1.0),
		    vmath::vec4(0.0, 1.0, 0.0, 1.0),
		    vmath::vec4(0.0, 0.0, 1.0, 1.0),
		    vmath::vec4(1.0, 1.0, 0.0, 1.0),
		    vmath::vec4(0.0, 1.0, 1.0, 1.0),
		    vmath::vec4(1.0, 0.0, 1.0, 1.0)};

		void initModelData()
		{
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][0][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[0][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][1][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[0][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][1][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[0][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][0][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[0][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][0][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[0][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][1][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[0][i]);}

			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][1][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[1][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][1][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[1][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][1][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[1][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][1][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[1][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][1][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[1][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][1][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[1][i]);}

			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][1][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[2][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][0][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[2][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][0][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[2][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][1][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[2][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][1][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[2][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][0][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[2][i]);}

			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][0][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[3][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][0][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[3][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][0][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[3][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][0][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[3][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][0][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[3][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][0][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[3][i]);}

			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][0][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[4][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][1][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[4][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][1][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[4][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][0][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[4][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][0][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[4][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][1][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[4][i]);}

			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][0][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[5][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][1][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[5][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][1][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[5][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][0][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[5][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[0][0][0][i] + mOffset[i]); mModelData.push_back(mBaseColors[5][i]);}
			for(int i = 0; i < 4; i++) {mModelData.push_back(mBaseVertices[1][1][1][i] + mOffset[i]); mModelData.push_back(mBaseColors[5][i]);}
		}

	public:
		Cube(vmath::vec4 _offset = vmath::vec4(0,0,0, 0)) : mOffset(_offset){
			initModelData();
			glBufferData(GL_ARRAY_BUFFER,
				     sizeof(mModelData.size()),
				     mModelData.data(),
				     GL_STATIC_DRAW);

			// 인터리브 레이아웃
			GLuint stride = 8 * sizeof(float);
			const void * vertexPointerOffset = (void *)0;
			const void * colorPointerOffset = (void *)sizeof(float);

			// attribute 0: 위치 (offset 0)
			glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, vertexPointerOffset);
			glEnableVertexAttribArray(0);
			// attribute 1: 색상 (offset 16)
			glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, colorPointerOffset);
			glEnableVertexAttribArray(1);
		};


	};

	class Camera : public ITransformable
	{
	private:
		vmath::mat4 mModelTransfomerMatrix;

		vmath::vec3 mEye;
		vmath::vec3 mTarget;
		vmath::vec3 mWorldUp;

		bool mIsPerspective = true;
		float fov;
		float near;
		float far;

	public:
		Camera() :
			mEye(5.0, 5.0, 5.0), mTarget(0.0, 0.0, 0.0), mWorldUp(0.0, 0.1, 0.0),
			fov(60), near(0.1), far(1000.0)
		{
		}

		bool CheckIsPerspective() { return mIsPerspective; }
		Camera &SetPerspective() { mIsPerspective = true; }
		Camera &SetOthogonal() { mIsPerspective = false; }

		vmath::mat4 GetLookAtMatrix() const
		{
			return vmath::lookat(mEye, mTarget, mWorldUp);
		}

		float GetAspectRatio(int window_width, int window_height) const
		{
			return (float)window_width / window_height;
		}

		vmath::mat4 GetProjectionMatrix(int window_width, int window_height) const
		{
			return mIsPerspective ? vmath::perspective(fov, GetAspectRatio(window_width, window_height), near, far) : vmath::ortho(-window_width / 2, window_width / 2, -window_height / 2, window_height / 2, near, far);
		}

		virtual vmath::vec3 GetPosition() const override { return mEye; }
		virtual void SetPosition(vmath::vec3 t) override {  mEye = t; }
		virtual void Translate(vmath::vec3 toward) override { mEye += toward; }
		virtual vmath::vec3 GetRotation() const override {return vmath::vec3(0,0,0); }
		virtual void SetRotation(vmath::vec3 euler) override { }
		virtual void Rotate(float angle, vmath::vec3 axis) override {  }
		virtual vmath::vec3 GetScale() const override { return vmath::vec3(0,0,0); }
		virtual void SetScale(vmath::vec3 s) override {  }
		virtual void Scale(vmath::vec3 adj) override { }
	};

	class my_application : public sb7::application
	{
	private:
		GLuint programAddr = 0;
		GLuint vaoAddr = 0;
		GLuint vboAddr = 0;

		const char *vs_path = "./resources/shaders/cube_vs.glsl";
		const char *fs_path = "./resources/shaders/cube_fs.glsl";

		std::vector<std::unique_ptr<Model>> models;
		Camera camera;

		GLuint create_shader(GLenum shader_type, const char *shader_path)
		{
			GLuint shader_addr = sb7::shader::load(shader_path, shader_type, true);
			if (shader_addr == 0)
				std::cerr << "쉐이더 로드 실패 : " << shader_path << std::endl;
			return shader_addr;
		}

		GLuint create_program()
		{
			GLuint program_addr = glCreateProgram();
			GLuint vshader = create_shader(GL_VERTEX_SHADER, vs_path);
			GLuint fshader = create_shader(GL_FRAGMENT_SHADER, fs_path);

			glAttachShader(program_addr, vshader);
			glAttachShader(program_addr, fshader);

			glLinkProgram(program_addr);

			glDeleteShader(vshader);
			glDeleteShader(fshader);
			return program_addr;
		}

	public:
		virtual void startup() override
		{
			programAddr = create_program();
			models.push_back(std::make_unique<Cube>(vmath::vec4(-0.5, 0, -0.5, 0)));
			camera.SetPosition(vmath::vec3(0.0f, 1.0f, 3.0f));
		}

		virtual void render(double currentTime) override
		{
			glClear(GL_COLOR_BUFFER_BIT);
			glEnable(GL_CULL_FACE);

			float angle = vmath::degrees((float)currentTime * 2);
			models.back()->SetRotation(vmath::vec3(0.0f, angle, 0.0f));
			models.back()->SetPosition(vmath::vec3((float)cos(currentTime * 2), 0.0f, (float)sin(currentTime * 2)));

			glUseProgram(programAddr);
			GLuint modelMatLocation = glGetUniformLocation(programAddr, "modelMat");
			GLuint lookatMatLocation = glGetUniformLocation(programAddr, "lookatMat");
			GLuint projMatLocation = glGetUniformLocation(programAddr, "projMat");
			glUniformMatrix4fv(modelMatLocation, 1, GL_FALSE, models.back()->GetModelTransformerMatrix());
			glUniformMatrix4fv(lookatMatLocation, 1, GL_FALSE, camera.GetLookAtMatrix());
			glUniformMatrix4fv(projMatLocation, 1, GL_FALSE, camera.GetProjectionMatrix(info.windowWidth, info.windowHeight));

			glBindVertexArray(vaoAddr);
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}

		virtual void shutdown() override
		{
		}
	};
}

DECLARE_MAIN(chapter6::my_application);
