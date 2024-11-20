# **pbEngine: Physics-Based Game Engine**

**pbEngine** is a simple game engine written in C++ where I experiment with technologies that interest me. The engine is designed as a sandbox for exploring advanced graphics, physics, and rendering techniques.

## **Features**

### **Graphics**
- **DirectX 12 Rendering Backend:**  
  The graphics pipeline is implemented using DirectX 12, enabling efficient rendering.
- **Mesh Shaders:**  
  Leverages the latest advancements in GPU programming for high-performance geometry processing.
- **Path Tracer with RTX:**  
  Implements a path tracing algorithm with real-time ray tracing support via NVIDIA RTX.
- **Real-Time Denoising:**  
  Utilizes **NVIDIA Real-Time Denoiser (NRD)** to remove noise from ray-traced images.
- **Upscaling with FSR3:**  
  Integrates AMD FidelityFX Super Resolution 3 (FSR3) for high-quality upscaling.

### **Physics**
- **NVIDIA PhysX:**  
  Powers physics simulations, including rigid body dynamics and collision detection.
- **NVIDIA Blast:**  
  Handles realistic physical destruction for objects in the scene.

### **Engine Design**
- **Entity-Component-System Architecture (ECS):**  
  Modular and scalable design for easy extension and system management.
- **Dear ImGui Integration:**  
  For GUI development, the engine incorporates **Dear ImGui**, making it efficient and developer-friendly.

## **Getting Started**

### **System Requirements**
- **Operating System:** Windows (only).  
  _Sorry, it’s game dev._  
- **Tools:** Visual Studio 2022.

### **Setup Instructions**
1. Clone the repository:
   ```bash
   git clone https://github.com/pavilbezpravil/pbEngineO.git
   ```
2. Navigate to the project directory:
   ```bash
   cd pbEngineO
   ```
3. Generate Visual Studio projects:
   ```bash
   ./Win-GenProjects VS22.bat
   ```
4. Open the generated solution in Visual Studio and build the project.

5. Run the editor:
   ```bash
   ./pbEditor.exe
   ```

6. Load a scene:
   For example, try the destructible showcase:
   ```bash
   ./assets/destruct_showcase.scn
   ```

## **License**
This project is licensed under the MIT License. See the `LICENSE` file for details.

---

If you have questions, suggestions, or just want to chat about game development, feel free to reach out via [Telegram](https://t.me/pavilbezpravil) or [email](mailto:pavilbezpravil@gmail.com).
