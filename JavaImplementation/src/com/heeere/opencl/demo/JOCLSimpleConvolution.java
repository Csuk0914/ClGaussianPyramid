package com.heeere.opencl.demo;

/*
 * JOCL - Java bindings for OpenCL
 *
 * Copyright 2009 Marco Hutter - http://www.jocl.org/
 */


import java.awt.BorderLayout;
import java.awt.Graphics;
import java.awt.GridLayout;
import java.awt.RenderingHints;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.geom.Point2D;
import java.awt.geom.Rectangle2D;
import java.awt.image.BufferedImage;
import java.awt.image.BufferedImageOp;
import java.awt.image.ColorModel;
import java.awt.image.ConvolveOp;
import java.awt.image.DataBufferInt;
import java.awt.image.Kernel;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;
import javax.imageio.ImageIO;
import javax.swing.ImageIcon;
import javax.swing.JComboBox;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.SwingUtilities;
import org.jocl.Pointer;
import org.jocl.Sizeof;
import org.jocl.cl_command_queue;
import org.jocl.cl_context;
import org.jocl.cl_context_properties;
import org.jocl.cl_device_id;
import org.jocl.cl_kernel;
import org.jocl.cl_mem;
import org.jocl.cl_platform_id;
import org.jocl.cl_program;

import static org.jocl.CL.*;


/**
 * A class that shows how a simple OpenCL implementation of a
 * convolution may be used as a BufferedImageOp that serves as
 * a hot-swap-replacement of the Java ConvolveOp.
 */
public class JOCLSimpleConvolution
{
    /**
     * Entry point for this sample.
     *
     * @param args not used
     */
    public static void main(String args[])
    {
        SwingUtilities.invokeLater(new Runnable()
        {
            public void run()
            {
                new JOCLSimpleConvolution();
            }
        });
    }

    /**
     * Creates a BufferedImage of with type TYPE_INT_RGB from the
     * file with the given name.
     *
     * @param fileName The file name
     * @return The image, or null if the file may not be read
     */
    private static BufferedImage createBufferedImage(String fileName)
    {
        BufferedImage image = null;
        try
        {
            image = ImageIO.read(new File(fileName));
        }
        catch (IOException e)
        {
            e.printStackTrace();
            return null;
        }

        int sizeX = image.getWidth();
        int sizeY = image.getHeight();

        BufferedImage result = new BufferedImage(
            sizeX, sizeY, BufferedImage.TYPE_INT_RGB);
        Graphics g = result.createGraphics();
        g.drawImage(image, 0, 0, null);
        g.dispose();
        return result;
    }

    /**
     * The JLabel which will display the computation time for Java
     */
    private JLabel javaTimeLabel;

    /**
     * The JLabel which will display the computation time for JOCL
     */
    private JLabel joclTimeLabel;

    /**
     * The input image to which the convolution will be applied
     */
    private BufferedImage inputImage;

    /**
     * The convoluted output image which is computed with Java
     */
    private BufferedImage outputImage0;

    /**
     * The convoluted output image which is computed with Java
     */
    private BufferedImage outputImage1;

    /**
     * The list of available kernels (not OpenCL kernels, but
     * java.awt.image.Kernel objects - the convolution kernels...)
     */
    private List<Kernel> kernels;

    /**
     * The list of names of the kernels which are displayed
     * in a combo box
     */
    private List<String> kernelNames;


    /**
     * Creates the JOCLSimpleConvolution sample
     */
    public JOCLSimpleConvolution()
    {
        // Read the input image file and create the output images
        String fileName = "../media/lena512color.png";

        inputImage = createBufferedImage(fileName);
        int sizeX = inputImage.getWidth();
        int sizeY = inputImage.getHeight();

        outputImage0 = new BufferedImage(
            sizeX, sizeY, BufferedImage.TYPE_INT_RGB);
        outputImage1 = new BufferedImage(
            sizeX, sizeY, BufferedImage.TYPE_INT_RGB);

        // Initialize the convolution kernels
        initKernels();

        final JPanel mainPanel = new JPanel(new GridLayout(1,0));
        JPanel panel = null;

        // Create the panel containing the input image and the combo box
        // for selecting the kernel
        final JComboBox kernelComboBox = new JComboBox(kernelNames.toArray());
        kernelComboBox.addActionListener(new ActionListener()
        {
            @Override
            public void actionPerformed(ActionEvent e)
            {
                int index = kernelComboBox.getSelectedIndex();
                Kernel kernel = kernels.get(index);
                applyKernel(kernel);
                mainPanel.repaint();
            }
        });
        panel = new JPanel(new BorderLayout());
        panel.add(new JLabel(new ImageIcon(inputImage)), BorderLayout.CENTER);
        panel.add(kernelComboBox, BorderLayout.NORTH);
        mainPanel.add(panel);

        // Create the panel containing the Java output image and time label
        javaTimeLabel = new JLabel();
        javaTimeLabel.setPreferredSize(kernelComboBox.getPreferredSize());
        panel = new JPanel(new BorderLayout());
        panel.add(new JLabel(new ImageIcon(outputImage0)), BorderLayout.CENTER);
        panel.add(javaTimeLabel, BorderLayout.NORTH);
        mainPanel.add(panel);

        // Create the panel containing the JOCL output image and time label
        joclTimeLabel = new JLabel();
        joclTimeLabel.setPreferredSize(kernelComboBox.getPreferredSize());
        panel = new JPanel(new BorderLayout());
        panel.add(new JLabel(new ImageIcon(outputImage1)), BorderLayout.CENTER);
        panel.add(joclTimeLabel, BorderLayout.NORTH);
        mainPanel.add(panel);

        // Create the main frame
        JFrame frame = new JFrame("JOCL Simple Convolution");
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        frame.setLayout(new BorderLayout());
        frame.add(mainPanel, BorderLayout.CENTER);
        frame.pack();
        frame.setVisible(true);

        kernelComboBox.setSelectedIndex(0);
    }



    /**
     * Apply the given kernel to the input image, once using a
     * Java ConvolveOp, and once using a JOCLConvolveOp
     *
     * @param kernel The kernel to apply
     */
    private void applyKernel(Kernel kernel)
    {
        long before = 0;
        long after = 0;
        double durationMS = 0;
        String message = null;
        BufferedImageOp op = null;

        // Apply the ConvolveOp and update the timing information
        op = new ConvolveOp(kernel);
        before = System.nanoTime();
        outputImage0 = op.filter(inputImage, outputImage0);
        after = System.nanoTime();
        durationMS = (after-before)/1e6;
        message = "Java: "+String.format("%.2f", durationMS)+" ms";
        System.out.println(message);
        javaTimeLabel.setText(message);

        // Apply the JOCLConvolveOp and update the timing information
        op = JOCLConvolveOp.create(kernel);
        before = System.nanoTime();
        outputImage1 = op.filter(inputImage, outputImage1);
        after = System.nanoTime();
        durationMS = (after-before)/1e6;
        message = "JOCL: "+String.format("%.2f", durationMS)+" ms";
        System.out.println(message);
        joclTimeLabel.setText(message);
    }

    /**
     * Initialize the list of available kernels and the list
     * containing their names, which will be displayed in
     * the combo box
     */
    private void initKernels()
    {
        kernels = new ArrayList<Kernel>();
        kernelNames = new ArrayList<String>();

        int kernelSizeX = 0;
        int kernelSizeY = 0;
        float kernelData[] = null;

        // Edge detection
        kernelSizeX = 3;
        kernelSizeY = 3;
        kernelData = new float[]
        {
            -1,  0, -1,
             0,  4,  0,
            -1,  0, -1
        };
        kernels.add(new Kernel(kernelSizeX, kernelSizeY, kernelData));
        kernelNames.add("Edge detection");

        // Sharpen
        kernelSizeX = 3;
        kernelSizeY = 3;
        kernelData = new float[]
        {
            -1,  0, -1,
             0,  5,  0,
            -1,  0, -1
        };
        kernels.add(new Kernel(kernelSizeX, kernelSizeY, kernelData));
        kernelNames.add("Sharpen");

        // Gaussian F
        kernelSizeX = 5;
        kernelSizeY = 5;
        kernelData = new float[]
        {
            1, 4, 6, 4, 1,
            4, 16, 24, 16, 4,
            6, 24, 36, 24, 6,
            4, 16, 24, 16, 4,
            1, 4, 6, 4, 1,
        };
        norm(kernelData);
        kernels.add(new Kernel(kernelSizeX, kernelSizeY, kernelData));
        kernelNames.add("Gauss");

        // Blur
        for (int i=3; i<=21; i+=2)
        {
            initBlurKernel(i);
        }
    }


    /**
     * Create a blur kernel with the given size, and add the
     * kernel and its name to the respective lists.
     */
    private void initBlurKernel(int kernelSize)
    {
        int kernelSizeX = kernelSize;
        int kernelSizeY = kernelSize;
        int size = kernelSizeX * kernelSizeY;
        float value = 1.0f / size;
        float kernelData[] = new float[size];
        for (int i=0; i<size; i++)
        {
            kernelData[i] = value;
        }
        kernels.add(new Kernel(kernelSizeX, kernelSizeY, kernelData));
        kernelNames.add("Blur "+kernelSizeX+"x"+kernelSizeY);
    }

    private void norm(float[] kernelData) {
        float sum = 0;
        for (float f : kernelData) {
            sum += f;
        }
        for (int i = 0; i < kernelData.length; i++) {
            kernelData[i] /= sum;
        }
    }


}


/**
 * This class is a BufferedImageOp which performs a convolution
 * using JOCL. For BufferedImages of type TYPE_INT_RGB it may
 * be used the same way as a Java ConvolveOp.
 */
class JOCLConvolveOp implements BufferedImageOp
{
    /**
     * The name of the source file for the OpenCL kernel
     */
    private static final String KERNEL_SOURCE_FILE_NAME =
        "../cl/SimpleConvolution.cl";

    /**
     * Compute the value which is the smallest multiple
     * of the given group size that is greater than or
     * equal to the given global size.
     *
     * @param groupSize The group size
     * @param globalSize The global size
     * @return The rounded global size
     */
    private static long round(long groupSize, long globalSize)
    {
        long r = globalSize % groupSize;
        if(r == 0)
        {
            return globalSize;
        } else
        {
            return globalSize + groupSize - r;
        }
    }

    /**
     * Helper function which reads the file with the given name and returns
     * the contents of this file as a String. Will exit the application
     * if the file can not be read.
     *
     * @param fileName The name of the file to read.
     * @return The contents of the file
     */
    private static String readFile(String fileName)
    {
        try
        {
            BufferedReader br = new BufferedReader(
                new InputStreamReader(new FileInputStream(fileName)));
            StringBuffer sb = new StringBuffer();
            String line = null;
            while (true)
            {
                line = br.readLine();
                if (line == null)
                {
                    break;
                }
                sb.append(line).append("\n");
            }
            return sb.toString();
        }
        catch (IOException e)
        {
            e.printStackTrace();
            System.exit(1);
            return null;
        }
    }

    /**
     * The OpenCL context
     */
    private cl_context context;

    /**
     * The OpenCL command queue
     */
    private cl_command_queue commandQueue;

    /**
     * The OpenCL kernel which will perform the convolution
     */
    private cl_kernel clKernel;

    /**
     * The kernel which is used for the convolution
     */
    private Kernel kernel;

    /**
     * The memory object that stores the kernel data
     */
    private cl_mem kernelMem;

    /**
     * The memory object for the input image
     */
    private cl_mem inputImageMem;

    /**
     * The memory object for the output image
     */
    private cl_mem outputImageMem;


    /**
     * Creates a new JOCLConvolveOp which may be used to apply the
     * given kernel to a BufferedImage. This method will create
     * an OpenCL context for the first platform that is found,
     * and a command queue for the first device that is found.
     * To create a JOCLConvolveOp for an existing context and
     * command queue, use the constructor of this class.
     *
     * @param kernel The kernel to apply
     * @return The JOCLConvolveOp for the given kernel.
     */
    public static JOCLConvolveOp create(Kernel kernel)
    {
        // Obtain the platform IDs and initialize the context properties
        cl_platform_id platforms[] = new cl_platform_id[1];
        clGetPlatformIDs(platforms.length, platforms, null);
        cl_context_properties contextProperties = new cl_context_properties();
        contextProperties.addProperty(CL_CONTEXT_PLATFORM, platforms[0]);

        // Create an OpenCL context on a GPU device
        cl_context context = clCreateContextFromType(
            contextProperties, CL_DEVICE_TYPE_GPU, null, null, null);
        if (context == null)
        {
            // If no context for a GPU device could be created,
            // try to create one for a CPU device.
            context = clCreateContextFromType(
                contextProperties, CL_DEVICE_TYPE_CPU, null, null, null);

            if (context == null)
            {
                System.out.println("Unable to create a context");
                System.exit(1);
                return null;
            }
        }

        // Enable exceptions and subsequently omit error checks in this sample
        setExceptionsEnabled(true);

        // Get the list of GPU devices associated with the context,
        // and create a command queue for the first device
        long numBytes[] = new long[1];
        clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, null, numBytes);
        int numDevices = (int) numBytes[0] / Sizeof.cl_device_id;
        cl_device_id devices[] = new cl_device_id[numDevices];
        clGetContextInfo(context, CL_CONTEXT_DEVICES, numBytes[0],
            Pointer.to(devices), null);
        cl_device_id device = devices[0];
        cl_command_queue commandQueue =
            clCreateCommandQueue(context, device, 0, null);

        return new JOCLConvolveOp(context, commandQueue, kernel);
    }


    /**
     * Creates a JOCLConvolveOp for the given context and command queue,
     * which may be used to apply the given kernel to a BufferedImage.
     *
     * @param context The context
     * @param commandQueue The command queue
     * @param kernel The kernel to apply
     */
    public JOCLConvolveOp(
        cl_context context, cl_command_queue commandQueue, Kernel kernel)
    {
        this.context = context;
        this.commandQueue = commandQueue;
        this.kernel = kernel;

        // Create the OpenCL kernel from the program
        String source = readFile(KERNEL_SOURCE_FILE_NAME);
        cl_program program = clCreateProgramWithSource(context, 1,
            new String[]{ source }, null, null);
        String compileOptions = "-cl-mad-enable";
        clBuildProgram(program, 0, null, compileOptions, null, null);
        clKernel = clCreateKernel(program, "convolution", null);

        // Create the ... other kernel... for the convolution
        float kernelData[] = kernel.getKernelData(null);
        kernelMem = clCreateBuffer(context, CL_MEM_READ_ONLY,
            kernelData.length * Sizeof.cl_uint, null, null);
        clEnqueueWriteBuffer(commandQueue, kernelMem,
            true, 0, kernelData.length * Sizeof.cl_uint,
            Pointer.to(kernelData), 0, null, null);

    }

    @Override
    public BufferedImage createCompatibleDestImage(
        BufferedImage src, ColorModel destCM)
    {
        int w = src.getWidth();
        int h = src.getHeight();
        BufferedImage result =
            new BufferedImage(w, h, BufferedImage.TYPE_INT_RGB);
        return result;
    }

    @Override
    public BufferedImage filter(BufferedImage src, BufferedImage dst)
    {
        // Validity checks for the given images
        if (src.getType() != BufferedImage.TYPE_INT_RGB)
        {
            throw new IllegalArgumentException(
                "Source image is not TYPE_INT_RGB");
        }
        if (dst == null)
        {
            dst = createCompatibleDestImage(src, null);
        }
        else if (dst.getType() != BufferedImage.TYPE_INT_RGB)
        {
            throw new IllegalArgumentException(
                "Destination image is not TYPE_INT_RGB");
        }
        if (src.getWidth() != dst.getWidth() ||
            src.getHeight() != dst.getHeight())
        {
            throw new IllegalArgumentException(
                "Images do not have the same size");
        }
        int imageSizeX = src.getWidth();
        int imageSizeY = src.getHeight();

        // Create the memory object for the input- and output image
        DataBufferInt dataBufferSrc =
            (DataBufferInt)src.getRaster().getDataBuffer();
        int dataSrc[] = dataBufferSrc.getData();
        inputImageMem = clCreateBuffer(context,
            CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
            dataSrc.length * Sizeof.cl_uint,
            Pointer.to(dataSrc), null);

        outputImageMem = clCreateBuffer(context, CL_MEM_WRITE_ONLY,
            imageSizeX * imageSizeY * Sizeof.cl_uint, null, null);

        // Set work sizes and arguments, and execute the kernel
        int kernelSizeX = kernel.getWidth();
        int kernelSizeY = kernel.getHeight();
        int kernelOriginX = kernel.getXOrigin();
        int kernelOriginY = kernel.getYOrigin();

        long localWorkSize[] = new long[2];
        localWorkSize[0] = kernelSizeX;
        localWorkSize[1] = kernelSizeY;

        long globalWorkSize[] = new long[2];
        globalWorkSize[0] = round(localWorkSize[0], imageSizeX);
        globalWorkSize[1] = round(localWorkSize[1], imageSizeY);

        int imageSize[] = new int[]{ imageSizeX, imageSizeY };
        int kernelSize[] = new int[]{ kernelSizeX, kernelSizeY };
        int kernelOrigin[] = new int[]{ kernelOriginX, kernelOriginY };

        clSetKernelArg(clKernel, 0, Sizeof.cl_mem, Pointer.to(inputImageMem));
        clSetKernelArg(clKernel, 1, Sizeof.cl_mem, Pointer.to(kernelMem));
        clSetKernelArg(clKernel, 2, Sizeof.cl_mem, Pointer.to(outputImageMem));
        clSetKernelArg(clKernel, 3, Sizeof.cl_int2, Pointer.to(imageSize));
        clSetKernelArg(clKernel, 4, Sizeof.cl_int2, Pointer.to(kernelSize));
        clSetKernelArg(clKernel, 5, Sizeof.cl_int2, Pointer.to(kernelOrigin));

        //System.out.println("global "+Arrays.toString(globalWorkSize));
        //System.out.println("local  "+Arrays.toString(localWorkSize));

        clEnqueueNDRangeKernel(commandQueue, clKernel, 2, null,
            globalWorkSize, localWorkSize, 0, null, null);

        // Read the pixel data into the BufferedImage
        DataBufferInt dataBufferDst =
            (DataBufferInt)dst.getRaster().getDataBuffer();
        int dataDst[] = dataBufferDst.getData();
        clEnqueueReadBuffer(commandQueue, outputImageMem,
            CL_TRUE, 0, dataDst.length * Sizeof.cl_uint,
            Pointer.to(dataDst), 0, null, null);

        // Clean up
        clReleaseMemObject(inputImageMem);
        clReleaseMemObject(outputImageMem);

        return dst;
    }

    @Override
    public Rectangle2D getBounds2D(BufferedImage src)
    {
        return src.getRaster().getBounds();
    }

    @Override
    public final Point2D getPoint2D(Point2D srcPt, Point2D dstPt)
    {
        if (dstPt == null)
        {
            dstPt = new Point2D.Float();
        }
        dstPt.setLocation(srcPt.getX(), srcPt.getY());
        return dstPt;
    }

    @Override
    public RenderingHints getRenderingHints()
    {
        return null;
    }

    @Override
    public void finalize() throws Throwable
    {
        clReleaseMemObject(kernelMem);
        super.finalize();
    }
}

